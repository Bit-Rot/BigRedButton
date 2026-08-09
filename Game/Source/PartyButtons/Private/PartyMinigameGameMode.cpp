#include "PartyMinigameGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // beep....beep....BOOOP — the third beat is the "GO!" that transitions to
    // Playing (see EnterPlaying). Times are seconds into the discovery phase.
    struct FCountdownBeat { float Time; bool bIsBoop; };
    constexpr FCountdownBeat GCountdownBeats[] = {
        { 1.0f, false },
        { 2.0f, false },
        { 3.0f, true  },
    };
    constexpr int32 GNumCountdownBeats = UE_ARRAY_COUNT(GCountdownBeats);
}

APartyMinigameGameMode::APartyMinigameGameMode()
{
    // Enabled so the intro state machine and TickAI can run. No GameMode in
    // this codebase ticked before this — scoped narrowly to minigames, not the
    // menu/lobby phases, which don't need it.
    PrimaryActorTick.bCanEverTick = true;

    // Placeholder countdown one-shots — a stock engine 1kHz sine ping, played
    // back at two different pitches for beep vs. BOOOP (see PlayCountdownBeat).
    // Designer-authored SoundBase assets can be plugged in via the
    // EditDefaultsOnly properties without touching this default.
    static ConstructorHelpers::FObjectFinder<USoundBase> ToneFinder(
        TEXT("/Engine/EngineSounds/1kSineTonePing.1kSineTonePing"));
    if (ToneFinder.Succeeded())
    {
        BeepSound = ToneFinder.Object;
        BoopSound = ToneFinder.Object;
    }
}

void APartyMinigameGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (IntroPhase == EPartyOverlayPhase::Playing)
    {
        TickAI(DeltaSeconds);
        return;
    }

    if (bWinnerDeclared) { return; }

    // Deferred to the first tick, not BeginPlay: subclasses spawn their pawns
    // AFTER their Super::BeginPlay() call, so a freeze issued from BeginPlay
    // would miss everything they're about to create. Costs at most one frame of
    // unfrozen gameplay before the tutorial dialog is even readable.
    if (!bIntroFreezeApplied)
    {
        bIntroFreezeApplied = true;
        SetGameplayFrozen(true);
    }

    IntroElapsed += DeltaSeconds;

    if (IntroPhase == EPartyOverlayPhase::Tutorial)
    {
        if (IntroElapsed >= TutorialMaxSeconds)
        {
            EnterDiscovery();
        }
        return;
    }

    // Discovery: fire countdown beats as they come due, then transition to
    // Playing on the BOOOP beat. Order matters — PlayCountdownBeat first so the
    // audio starts on the same frame the game unpauses (spec: "the game starts
    // as the BOOOP plays (not after)").
    while (NextCountdownBeat < GNumCountdownBeats &&
           IntroElapsed >= GCountdownBeats[NextCountdownBeat].Time)
    {
        const bool bIsBoop = GCountdownBeats[NextCountdownBeat].bIsBoop;
        PlayCountdownBeat(bIsBoop);
        ++NextCountdownBeat;
        if (bIsBoop)
        {
            EnterPlaying();
            return;
        }
    }
}

void APartyMinigameGameMode::BeginPlay()
{
    Super::BeginPlay();

    bWinnerDeclared   = false;
    CurrentRosterIndex = INDEX_NONE;
    CurrentGameName   = TEXT("???");

    IntroPhase          = EPartyOverlayPhase::Tutorial;
    IntroElapsed        = 0.f;
    NextCountdownBeat   = 0;
    bIntroFreezeApplied = false;
    HeldButtons.Reset();
    HumanParticipants.Reset();

    if (!GetWorld())
    {
        return;
    }

    // Resolve the current game from the map name.
    // PIE prefixes the map name with "UEDPIE_0_"; strip it before the roster lookup.
    FString RawName = GetWorld()->GetMapName();
    UWorld::RemovePIEPrefix(RawName);

    // RawName may be a full package path like "/Game/Maps/L_GameA" or just "L_GameA".
    // We only need the short name (the last component after '/').
    FString ShortName = RawName;
    int32 SlashIndex  = INDEX_NONE;
    if (RawName.FindLastChar(TEXT('/'), SlashIndex))
    {
        ShortName = RawName.Mid(SlashIndex + 1);
    }

    if (UPartySessionSubsystem* S = Session())
    {
        CurrentRosterIndex = S->FindGameByMapName(FName(*ShortName));
        if (CurrentRosterIndex != INDEX_NONE)
        {
            const TArray<FPartyGameInfo>& Roster = S->GetRoster();
            CurrentGameName = Roster[CurrentRosterIndex].DisplayName;
        }
    }

    if (CurrentRosterIndex == INDEX_NONE)
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("APartyMinigameGameMode: could not find roster entry for map '%s' (raw: '%s')."),
            *ShortName, *RawName);
    }
    else
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyMinigameGameMode: running game %d — %s."),
            CurrentRosterIndex, *CurrentGameName);
    }
}

FString APartyMinigameGameMode::GetHudTitle() const
{
    return CurrentGameName;
}

float APartyMinigameGameMode::GetTutorialRemainingSeconds() const
{
    if (IntroPhase != EPartyOverlayPhase::Tutorial) { return -1.f; }
    return FMath::Max(0.f, TutorialMaxSeconds - IntroElapsed);
}

// ---- Input: the intro gate --------------------------------------------------

void APartyMinigameGameMode::OnPlayerButton(int32 PlayerIndex)
{
    HeldButtons.Add(PlayerIndex);

    switch (IntroPhase)
    {
    case EPartyOverlayPhase::Tutorial:
        // Ready-check dismiss: every human is now holding → skip the rest of
        // the timer. (No humans registered? Only the timer dismisses.)
        if (AllHumansHolding())
        {
            EnterDiscovery();
        }
        return;

    case EPartyOverlayPhase::Discovery:
        // Held state is read by GetPawnMarkers() → HUD tints the number label.
        // No gameplay action here — the round hasn't started.
        return;

    default:
        OnGameplayButton(PlayerIndex);
        return;
    }
}

void APartyMinigameGameMode::OnPlayerButtonReleased(int32 PlayerIndex)
{
    HeldButtons.Remove(PlayerIndex);

    if (IntroPhase != EPartyOverlayPhase::Playing) { return; }

    OnGameplayButtonReleased(PlayerIndex);
}

void APartyMinigameGameMode::OnGameplayButton(int32 PlayerIndex)
{
    if (bWinnerDeclared) { return; }

    // Only registered players can win.
    if (!IsTileJoined(PlayerIndex))
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyMinigameGameMode: player %d pressed but is not registered — ignoring."),
            PlayerIndex + 1);
        return;
    }

    DeclareWinner(PlayerIndex);
}

// ---- Intro overlay ----------------------------------------------------------

void APartyMinigameGameMode::RegisterParticipant(int32 PlayerIndex, bool bIsAI)
{
    if (!bIsAI)
    {
        HumanParticipants.Add(PlayerIndex);
    }
}

bool APartyMinigameGameMode::AllHumansHolding() const
{
    if (HumanParticipants.IsEmpty()) { return false; }
    for (int32 P : HumanParticipants)
    {
        if (!HeldButtons.Contains(P)) { return false; }
    }
    return true;
}

void APartyMinigameGameMode::EnterDiscovery()
{
    if (IntroPhase != EPartyOverlayPhase::Tutorial) { return; }

    IntroPhase        = EPartyOverlayPhase::Discovery;
    IntroElapsed      = 0.f;
    NextCountdownBeat = 0;

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyMinigameGameMode: tutorial dismissed — entering discovery countdown."));
}

void APartyMinigameGameMode::EnterPlaying()
{
    if (IntroPhase == EPartyOverlayPhase::Playing) { return; }

    IntroPhase = EPartyOverlayPhase::Playing;
    SetGameplayFrozen(false);

    // Any button already held when the BOOOP fires counts as a real press, for
    // games that want it — see ShouldReplayHeldButtonsOnStart for why this is
    // opt-in rather than the default.
    if (ShouldReplayHeldButtonsOnStart())
    {
        for (int32 PlayerIndex : HeldButtons)
        {
            OnGameplayButton(PlayerIndex);
        }
    }

    OnGameplayStarted();

    UE_LOG(LogPartyButtons, Log, TEXT("APartyMinigameGameMode: GO! — discovery ended, game live."));
}

void APartyMinigameGameMode::PlayCountdownBeat(bool bIsBoop)
{
    USoundBase* Sound = bIsBoop ? BoopSound : BeepSound;
    if (!Sound)
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("APartyMinigameGameMode: countdown %s sound not set — skipping."),
            bIsBoop ? TEXT("BOOOP") : TEXT("beep"));
        return;
    }

    // Pitch-shift the same tone so beep vs. BOOOP is unambiguous: lead-in
    // beeps are bright and quiet, the BOOOP is deeper and louder.
    const float Volume = bIsBoop ? 1.0f : 0.6f;
    const float Pitch  = bIsBoop ? 0.5f : 1.4f;
    UGameplayStatics::PlaySound2D(this, Sound, Volume, Pitch);
}

// ---- Win / travel -----------------------------------------------------------

void APartyMinigameGameMode::DeclareWinner(int32 PlayerIndex)
{
    if (bWinnerDeclared) { return; }
    bWinnerDeclared = true;

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyMinigameGameMode: player %d wins game %d (%s)!"),
        PlayerIndex + 1, CurrentRosterIndex, *CurrentGameName);

    if (UPartySessionSubsystem* S = Session())
    {
        S->RecordWin(PlayerIndex);
    }

    AdvanceAndTravel();
}

void APartyMinigameGameMode::DeclareNoContest()
{
    if (bWinnerDeclared) { return; }
    bWinnerDeclared = true;

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyMinigameGameMode: game %d (%s) ended with no winner."),
        CurrentRosterIndex, *CurrentGameName);

    AdvanceAndTravel();
}

void APartyMinigameGameMode::AdvanceAndTravel()
{
    UPartySessionSubsystem* S = Session();
    if (!S) { return; }

    S->AdvanceGame();

    if (S->IsSessionComplete())
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyMinigameGameMode: session complete (%d games) — going to Results."),
            S->GetGamesPlayed());
        TravelToPhase(EPartyPhase::Results);
    }
    else
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyMinigameGameMode: game %d/%d done — back to LevelSelect."),
            S->GetGamesPlayed(), S->GetGamesPerSession());
        TravelToPhase(EPartyPhase::LevelSelect);
    }
}
