#include "PartyArenaGameMode.h"
#include "PartyButtons.h"
#include "PartyArena.h"
#include "PartyDuelPawn.h"
#include "PartyBullet.h"
#include "PartySessionSubsystem.h"
#include "PartySessionState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator, used by ShouldBlockNow to scan live bullets

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
    Super::BeginPlay(); // resolves CurrentRosterIndex / CurrentGameName, resets the intro overlay

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
        return;
    }

    // Pawns are frozen until the countdown BOOOP by the inherited intro overlay
    // (see SetGameplayFrozen) — visible but static, so players have a still
    // target to identify above their number label.
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

    // Seed from wallclock so spawn positions and AI timing differ each game.
    // GetWorld()->GetUniqueID() is a UObject counter that lands on the same
    // value across fresh launches, which made every match play out identically.
    const int32 SeedBase = static_cast<int32>(FDateTime::Now().GetTicks());
    FRandomStream Rng(SeedBase ^ 0x50415254);
    AIRng = FRandomStream(SeedBase ^ 0x41494152); // "AIR"

    // Cached once so ricochet planning (PlanShot) knows how far bullets bounce
    // off the wall surface without needing a live bullet instance to ask.
    if (const APartyBullet* BulletCDO = GetDefault<APartyBullet>())
    {
        BulletRadiusUnitsForPlanning = BulletCDO->GetRadiusUnits();
    }

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
            const PartyDuelAI::FDuelAIParams AIParams = GetActiveAIParams();

            FDuelAIState& AIPawnState = AIState.FindOrAdd(PlayerIndex);
            AIPawnState.bHolding         = false;
            AIPawnState.bPlanCommitted   = false;
            AIPawnState.bBlockPending    = false;
            // Staggered start so every AI pawn doesn't decide on the same frame.
            AIPawnState.NextDecisionTime = GetWorld()->GetTimeSeconds()
                + AIRng.FRandRange(AIParams.ReactionLatencySeconds, AIParams.ReactionLatencySeconds * 2.f);
        }
        // The tutorial's "all humans holding" ready check waits on every non-AI
        // participant — this covers registered lobby players AND the dev
        // fallback set, so opening L_GameA directly still exits the tutorial
        // the moment the fallback participants press.
        RegisterParticipant(PlayerIndex, AIParticipants.Contains(PlayerIndex));
    }

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyArenaGameMode: spawned %d duel pawn(s) (%d AI)."),
        Pawns.Num(), AIParticipants.Num());
}

void APartyArenaGameMode::TickAI(float DeltaSeconds)
{
    // The base only calls TickAI during Playing, so AI stays silent through the
    // intro overlay for free — same "no game logic runs until the BOOOP" rule
    // the human-facing OnPlayerButton gate enforces.
    if (bWinnerDeclared || AIState.IsEmpty() || !GetWorld()) { return; }

    const double Now = GetWorld()->GetTimeSeconds();
    const PartyDuelAI::FDuelAIParams Params = GetActiveAIParams();

    // Copy keys first: OnPlayerButton/OnPlayerButtonReleased below can lead to
    // a pawn dying (immediately or on a later tick) and HandlePawnDied mutating
    // AIState — iterating a snapshot avoids invalidating the loop.
    TArray<int32> Indices;
    AIState.GetKeys(Indices);

    for (int32 PlayerIndex : Indices)
    {
        FDuelAIState* State = AIState.Find(PlayerIndex);
        TObjectPtr<APartyDuelPawn>* PawnPtr = Pawns.Find(PlayerIndex);
        if (!State || !PawnPtr || !*PawnPtr) { continue; }
        APartyDuelPawn& Pawn = **PawnPtr;

        // ---- Finish a pending block tap (pressed last tick; release now — a
        // one-tick hold is comfortably inside TapWindowSeconds, so it always
        // classifies as Reflect, never Cancel). ----
        if (State->bBlockPending)
        {
            OnPlayerButtonReleased(PlayerIndex);
            State->bBlockPending    = false;
            State->NextDecisionTime = Now + AIRng.FRandRange(Params.ReactionLatencySeconds, Params.ReactionLatencySeconds * 2.f);
            continue;
        }

        // ---- Already charging a committed shot: just watch for release time. ----
        if (State->bHolding)
        {
            if (Now >= State->PressTime + State->PlannedHoldSeconds)
            {
                OnPlayerButtonReleased(PlayerIndex);
                State->bHolding      = false;
                State->bPlanCommitted = false;
                State->NextDecisionTime = Now + AIRng.FRandRange(Params.ReactionLatencySeconds, Params.ReactionLatencySeconds * 2.f);
            }
            continue; // can't block mid-charge — matches what a real player could do
        }

        // ---- Defense: only reachable while not charging, same as a human. ----
        if (ShouldBlockNow(Pawn, Params))
        {
            OnPlayerButton(PlayerIndex); // quick tap; released next tick above
            State->bBlockPending = true;
            continue;
        }

        // ---- Offense: reaction-latency gate keeps this from re-deciding every frame. ----
        if (Now < State->NextDecisionTime) { continue; }

        if (!State->bPlanCommitted)
        {
            if (!PlanShot(PlayerIndex, Pawn, Params, Now, *State)) { continue; }
            State->bPlanCommitted = true;
        }

        // Aiming == timing the press to the spin (see PartyDuelPawn — pressing
        // freezes facing). Press once the spinning facing is within tolerance
        // of the committed, jittered bearing.
        const float FacingDeg = Pawn.GetActorRotation().Yaw;
        if (FMath::Abs(PartyDuelAI::SignedAngleDeltaDeg(FacingDeg, State->PlannedBearingDeg)) <= Params.AimToleranceDeg)
        {
            OnPlayerButton(PlayerIndex);
            State->bHolding  = true;
            State->PressTime = Now;
        }
    }
}

PartyDuelAI::FDuelAIParams APartyArenaGameMode::GetActiveAIParams() const
{
    switch (AIDifficulty)
    {
    case EPartyAIDifficulty::Easy: return PartyDuelAI::EasyParams();
    case EPartyAIDifficulty::Hard: return PartyDuelAI::HardParams();
    case EPartyAIDifficulty::Medium:
    default:                       return PartyDuelAI::MediumParams();
    }
}

APartyDuelPawn* APartyArenaGameMode::FindNearestOpponent(int32 SelfIndex) const
{
    const TObjectPtr<APartyDuelPawn>* SelfPtr = Pawns.Find(SelfIndex);
    if (!SelfPtr || !*SelfPtr) { return nullptr; }
    const FVector SelfLoc = (*SelfPtr)->GetActorLocation();

    APartyDuelPawn* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    for (int32 OtherIndex : AlivePlayers)
    {
        if (OtherIndex == SelfIndex) { continue; }
        const TObjectPtr<APartyDuelPawn>* OtherPtr = Pawns.Find(OtherIndex);
        if (!OtherPtr || !*OtherPtr) { continue; }

        const float DistSq = FVector::DistSquared((*OtherPtr)->GetActorLocation(), SelfLoc);
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            Best = *OtherPtr;
        }
    }
    return Best;
}

bool APartyArenaGameMode::ShouldBlockNow(const APartyDuelPawn& SelfPawn, const PartyDuelAI::FDuelAIParams& Params) const
{
    if (!GetWorld()) { return false; }

    const FVector2D SelfPos2D(SelfPawn.GetActorLocation());
    const float CombinedRadius = SelfPawn.GetSphereRadiusUnits() + BulletRadiusUnitsForPlanning;

    for (TActorIterator<APartyBullet> It(GetWorld()); It; ++It)
    {
        APartyBullet* Bullet = *It;
        if (!Bullet) { continue; }

        // Don't try to block a bullet that's currently your own freshly-fired
        // shot — a human wouldn't reflexively parry their own gunshot either.
        if (Cast<APartyDuelPawn>(Bullet->GetOwner()) == &SelfPawn) { continue; }

        const FVector2D BulletPos2D(Bullet->GetActorLocation());
        const FVector2D BulletVel2D(Bullet->GetVelocity());

        const float TimeToImpact = PartyDuelAI::TimeToImpactSeconds(BulletPos2D, BulletVel2D, SelfPos2D, CombinedRadius);
        if (TimeToImpact < 0.f || TimeToImpact > Params.BlockThreatHorizonSeconds) { continue; }

        if (AIRng.FRand() < Params.BlockWhenThreatenedProb)
        {
            return true;
        }
    }
    return false;
}

bool APartyArenaGameMode::PlanShot(int32 SelfIndex, const APartyDuelPawn& SelfPawn, const PartyDuelAI::FDuelAIParams& Params, double Now, FDuelAIState& State)
{
    APartyDuelPawn* Target = FindNearestOpponent(SelfIndex);
    if (!Target) { return false; }

    if (AIRng.FRand() > Params.FireProbability)
    {
        // Hesitate rather than re-rolling every frame — reads as a human
        // pausing instead of an AI stuck in a decision loop.
        State.NextDecisionTime = Now + AIRng.FRandRange(0.15f, 0.4f);
        return false;
    }

    const FVector2D ShooterPos2D(SelfPawn.GetActorLocation());
    const FVector2D TargetPos2D(Target->GetActorLocation());

    float BearingDeg = PartyDuelAI::BearingDegXY(ShooterPos2D, TargetPos2D);

    const bool bAttemptRicochet = Arena && AIRng.FRand() < Params.RicochetProbability;
    if (bAttemptRicochet)
    {
        const float BulletRadiusMeters = BulletRadiusUnitsForPlanning / 100.f;
        const FBox2D BounceBox = Arena->GetPlayBounds(BulletRadiusMeters);

        float RicochetBearingDeg = BearingDeg;
        if (PartyDuelAI::PlanRicochetBearing(ShooterPos2D, TargetPos2D, BounceBox, Params.MaxRicochetBounces, AIRng, RicochetBearingDeg))
        {
            BearingDeg = RicochetBearingDeg;
        }
        // else: geometry didn't work out (rare, near-degenerate positions) — fall back to the direct shot already computed above.
    }

    State.PlannedBearingDeg = PartyDuelAI::ApplyAimJitter(BearingDeg, Params.AimErrorStdDevDeg, AIRng);

    const float ChargeFrac = FMath::Clamp(Params.PreferredChargeFrac + AIRng.FRandRange(-0.15f, 0.15f), 0.f, 1.f);
    State.PlannedHoldSeconds = PartyDuelAI::HoldSecondsForChargeFrac(ChargeFrac, SelfPawn.GetMinChargeSeconds(), SelfPawn.GetMaxChargeSeconds());

    return true;
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

void APartyArenaGameMode::OnGameplayButton(int32 PlayerIndex)
{
    if (TObjectPtr<APartyDuelPawn>* Found = Pawns.Find(PlayerIndex))
    {
        if (APartyDuelPawn* Pawn = *Found) { Pawn->NotifyPressed(); }
    }
}

void APartyArenaGameMode::OnGameplayButtonReleased(int32 PlayerIndex)
{
    if (TObjectPtr<APartyDuelPawn>* Found = Pawns.Find(PlayerIndex))
    {
        if (APartyDuelPawn* Pawn = *Found) { Pawn->NotifyReleased(); }
    }
}

// ---- Intro overlay hooks ----------------------------------------------------

void APartyArenaGameMode::SetGameplayFrozen(bool bFrozen)
{
    for (const TPair<int32, TObjectPtr<APartyDuelPawn>>& Pair : Pawns)
    {
        if (APartyDuelPawn* Pawn = Pair.Value.Get())
        {
            Pawn->SetActorTickEnabled(!bFrozen);
        }
    }
}

void APartyArenaGameMode::OnGameplayStarted()
{
    // Clear any half-formed AI plan and re-arm the reaction gate so nobody
    // fires on the exact start frame — same seeding SpawnPawns does.
    if (!GetWorld()) { return; }

    const PartyDuelAI::FDuelAIParams Params = GetActiveAIParams();
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto& Pair : AIState)
    {
        Pair.Value.bHolding         = false;
        Pair.Value.bPlanCommitted   = false;
        Pair.Value.bBlockPending    = false;
        Pair.Value.NextDecisionTime = Now
            + AIRng.FRandRange(Params.ReactionLatencySeconds, Params.ReactionLatencySeconds * 2.f);
    }
}

// ---- HUD-facing accessors ---------------------------------------------------

TArray<FPartyPawnMarker> APartyArenaGameMode::GetPawnMarkers() const
{
    TArray<FPartyPawnMarker> Markers;
    if (GetOverlayPhase() != EPartyOverlayPhase::Discovery) { return Markers; }

    Markers.Reserve(Pawns.Num());
    for (const TPair<int32, TObjectPtr<APartyDuelPawn>>& Pair : Pawns)
    {
        const APartyDuelPawn* Pawn = Pair.Value.Get();
        if (!Pawn) { continue; }

        FPartyPawnMarker M;
        M.WorldLocation = Pawn->GetActorLocation();
        M.PlayerIndex   = Pair.Key;
        M.Color         = PlayerColor(Pair.Key);
        M.bHeld         = GetHeldButtons().Contains(Pair.Key);
        Markers.Add(M);
    }
    return Markers;
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
