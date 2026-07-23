#include "PartyArenaGameMode.h"
#include "PartyButtons.h"
#include "PartyArena.h"
#include "PartyDuelPawn.h"
#include "PartySessionSubsystem.h"
#include "PartySessionState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

APartyArenaGameMode::APartyArenaGameMode()
{
    DefaultPawnClass = nullptr; // pawns are spawned manually and never possessed (see APartyDuelPawn class comment)
    ArenaClass        = APartyArena::StaticClass();
    DuelPawnClass     = APartyDuelPawn::StaticClass();
}

FString APartyArenaGameMode::GetHudSubtitle() const
{
    return TEXT("Idle: spin | Hold: charge, release to shoot | Quick tap: reflect");
}

void APartyArenaGameMode::BeginPlay()
{
    Super::BeginPlay(); // resolves CurrentRosterIndex / CurrentGameName for the HUD

    SpawnArena();
    SpawnPawns();

    // A duel needs 2+ combatants — guard degenerate configs (DevFallbackPlayers
    // misconfigured to 0/1, or exactly one player somehow registered) so the
    // round still resolves instead of hanging forever with no deaths to react to.
    if (AlivePlayers.Num() <= 1)
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyArenaGameMode: only %d participant(s) — resolving immediately."),
            AlivePlayers.Num());

        if (AlivePlayers.Num() == 1)
        {
            DeclareWinner(*AlivePlayers.CreateConstIterator());
        }
        else
        {
            DeclareNoContest();
        }
    }
}

void APartyArenaGameMode::SpawnArena()
{
    if (!GetWorld() || !ArenaClass) { return; }

    Arena = GetWorld()->SpawnActor<APartyArena>(ArenaClass, FTransform::Identity);
    if (!Arena)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("APartyArenaGameMode: failed to spawn arena."));
        return;
    }

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->SetViewTarget(Arena);
    }
}

void APartyArenaGameMode::SpawnPawns()
{
    if (!GetWorld() || !Arena || !DuelPawnClass) { return; }

    UPartySessionSubsystem* S = Session();

    TArray<bool> HumanClaimed;
    HumanClaimed.Init(false, FPartySessionState::NUM_PLAYERS);
    if (S)
    {
        for (int32 i = 0; i < FPartySessionState::NUM_PLAYERS; i++)
        {
            HumanClaimed[i] = S->IsPlayerRegistered(i);
        }
    }

    TArray<int32> Participants;
    for (int32 i = 0; i < FPartySessionState::NUM_PLAYERS; i++)
    {
        if (HumanClaimed[i]) { Participants.Add(i); }
    }

    // AI slots (dev-only Lobby Up/Down control) — derived the same way the
    // Lobby HUD derives them, so "player intent wins" and "reverse fill" are
    // consistent everywhere. See FPartySessionState::ComputeAISlots.
    const int32 NumAI = S ? S->GetNumAIPlayers() : 0;
    TArray<bool> AISlots;
    FPartySessionState::ComputeAISlots(HumanClaimed, NumAI, AISlots);
    for (int32 i = 0; i < AISlots.Num(); i++)
    {
        if (AISlots[i])
        {
            Participants.Add(i);
            AIParticipants.Add(i);
        }
    }

    // Dev-fallback: ONLY when NEITHER humans nor AI are configured (e.g.
    // opening L_GameA directly in PIE with nothing set up). Any AI count > 0
    // supersedes this — the explicit AI configuration always takes priority
    // over the synthetic fallback.
    if (Participants.IsEmpty())
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyArenaGameMode: no registered players or AI — spawning %d dev-fallback player(s)."),
            DevFallbackPlayers);
        for (int32 i = 0; i < DevFallbackPlayers; i++)
        {
            Participants.Add(i);
        }
    }

    // Seed varies per world instance (PIE session) but is otherwise deterministic —
    // consistent with FPartySessionState::SelectNextGame's use of FRandomStream
    // rather than true nondeterministic randomness.
    FRandomStream Rng(static_cast<int32>(GetWorld()->GetUniqueID()) ^ 0x50415254);
    AIRng = FRandomStream(static_cast<int32>(GetWorld()->GetUniqueID()) ^ 0x41494152); // "AIR"

    constexpr int32 MaxSpawnAttempts = 30;
    const float MinSepUnits = MinSpawnSeparationMeters * 100.f;

    TArray<FVector> PlacedLocations;

    for (int32 PlayerIndex : Participants)
    {
        FTransform SpawnTransform = Arena->GetRandomSpawnTransform(Rng, PlayerRadiusMeters);

        for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; Attempt++)
        {
            bool bTooClose = false;
            for (const FVector& Placed : PlacedLocations)
            {
                if (FVector::DistSquared(Placed, SpawnTransform.GetLocation()) < FMath::Square(MinSepUnits))
                {
                    bTooClose = true;
                    break;
                }
            }
            if (!bTooClose) { break; }
            SpawnTransform = Arena->GetRandomSpawnTransform(Rng, PlayerRadiusMeters);
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        APartyDuelPawn* Pawn = GetWorld()->SpawnActor<APartyDuelPawn>(DuelPawnClass, SpawnTransform, Params);
        if (!Pawn) { continue; }

        Pawn->Init(PlayerIndex, PlayerColor(PlayerIndex));
        Pawn->OnDied.AddUObject(this, &APartyArenaGameMode::HandlePawnDied);

        Pawns.Add(PlayerIndex, Pawn);
        AlivePlayers.Add(PlayerIndex);
        PlacedLocations.Add(SpawnTransform.GetLocation());

        if (AIParticipants.Contains(PlayerIndex))
        {
            FDuelAIState& AIPawnState = AIState.FindOrAdd(PlayerIndex);
            AIPawnState.bHolding      = false;
            AIPawnState.NextEventTime = GetWorld()->GetTimeSeconds() + AIRng.FRandRange(AIThinkMinSeconds, AIThinkMaxSeconds);
        }
    }

    UE_LOG(LogPartyButtons, Log, TEXT("APartyArenaGameMode: spawned %d duel pawn(s) (%d AI)."),
        Pawns.Num(), AIParticipants.Num());
}

void APartyArenaGameMode::TickAI(float DeltaSeconds)
{
    if (bWinnerDeclared || AIState.IsEmpty() || !GetWorld()) { return; }

    const double Now = GetWorld()->GetTimeSeconds();

    // Copy keys first: OnPlayerButton/OnPlayerButtonReleased below can lead to
    // a pawn dying (immediately or on a later tick) and HandlePawnDied mutating
    // AIState — iterating a snapshot avoids invalidating the loop.
    TArray<int32> Indices;
    AIState.GetKeys(Indices);

    for (int32 PlayerIndex : Indices)
    {
        FDuelAIState* State = AIState.Find(PlayerIndex);
        if (!State || Now < State->NextEventTime) { continue; }

        if (!State->bHolding)
        {
            // Same entry point real button input uses (via the delegate chain
            // from APartyInputController) — see this method's class comment.
            OnPlayerButton(PlayerIndex);
            State->bHolding      = true;
            State->NextEventTime = Now + AIRng.FRandRange(AIHoldMinSeconds, AIHoldMaxSeconds);
        }
        else
        {
            OnPlayerButtonReleased(PlayerIndex);
            State->bHolding      = false;
            State->NextEventTime = Now + AIRng.FRandRange(AIThinkMinSeconds, AIThinkMaxSeconds);
        }
    }
}

FLinearColor APartyArenaGameMode::PlayerColor(int32 PlayerIndex)
{
    static const FLinearColor Palette[] = {
        FLinearColor(0.95f, 0.15f, 0.15f), FLinearColor(0.15f, 0.55f, 0.95f),
        FLinearColor(0.20f, 0.85f, 0.25f), FLinearColor(0.95f, 0.85f, 0.10f),
        FLinearColor(0.80f, 0.20f, 0.85f), FLinearColor(0.95f, 0.55f, 0.10f),
        FLinearColor(0.15f, 0.85f, 0.80f), FLinearColor(0.95f, 0.45f, 0.65f),
        FLinearColor(0.55f, 0.35f, 0.15f), FLinearColor(0.65f, 0.65f, 0.65f),
        FLinearColor(0.35f, 0.15f, 0.85f), FLinearColor(0.45f, 0.75f, 0.15f),
        FLinearColor(0.85f, 0.25f, 0.45f), FLinearColor(0.10f, 0.35f, 0.65f),
        FLinearColor(0.75f, 0.75f, 0.15f), FLinearColor(0.35f, 0.85f, 0.55f),
    };
    return Palette[PlayerIndex % UE_ARRAY_COUNT(Palette)];
}

void APartyArenaGameMode::OnPlayerButton(int32 PlayerIndex)
{
    if (TObjectPtr<APartyDuelPawn>* Found = Pawns.Find(PlayerIndex))
    {
        if (APartyDuelPawn* Pawn = *Found) { Pawn->NotifyPressed(); }
    }
}

void APartyArenaGameMode::OnPlayerButtonReleased(int32 PlayerIndex)
{
    if (TObjectPtr<APartyDuelPawn>* Found = Pawns.Find(PlayerIndex))
    {
        if (APartyDuelPawn* Pawn = *Found) { Pawn->NotifyReleased(); }
    }
}

void APartyArenaGameMode::HandlePawnDied(int32 PlayerIndex)
{
    if (bWinnerDeclared) { return; } // round already resolved — ignore stragglers

    AlivePlayers.Remove(PlayerIndex);
    Pawns.Remove(PlayerIndex);
    AIState.Remove(PlayerIndex); // avoid stale timer churn for a dead AI pawn

    UE_LOG(LogPartyButtons, Log, TEXT("APartyArenaGameMode: player %d down — %d remaining."),
        PlayerIndex + 1, AlivePlayers.Num());

    if (AlivePlayers.Num() == 1)
    {
        DeclareWinner(*AlivePlayers.CreateConstIterator());
    }
    else if (AlivePlayers.Num() == 0)
    {
        // Simultaneous deaths (e.g. two bullets connect the same frame) — a draw.
        DeclareNoContest();
    }
}
