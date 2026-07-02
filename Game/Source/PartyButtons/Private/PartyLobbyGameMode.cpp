#include "PartyLobbyGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"
#include "TimerManager.h"

APartyLobbyGameMode::APartyLobbyGameMode()
{
    bCountingDown    = false;
    CountdownEndTime = 0.0f;
}

void APartyLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Initialize per-button held-state tracking.
    bButtonHeld.Init(false, 16);

    UE_LOG(LogPartyButtons, Log, TEXT("APartyLobbyGameMode: lobby open — hold 2+ buttons to start."));
}

// ---- HUD accessors ---------------------------------------------------------

float APartyLobbyGameMode::GetCountdownRemaining() const
{
    if (!bCountingDown || !GetWorld())
    {
        return -1.0f;
    }
    return FMath::Max(0.0f, CountdownEndTime - GetWorld()->GetTimeSeconds());
}

bool APartyLobbyGameMode::IsTileJoined(int32 i) const
{
    // "Joined" in lobby = currently holding the button.
    return bButtonHeld.IsValidIndex(i) && bButtonHeld[i];
}

// ---- Button events ---------------------------------------------------------

void APartyLobbyGameMode::OnPlayerButton(int32 PlayerIndex)
{
    if (!bButtonHeld.IsValidIndex(PlayerIndex)) { return; }

    bButtonHeld[PlayerIndex] = true;

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyLobbyGameMode: player %d pressed — %d held."),
        PlayerIndex + 1, GetActiveHeldCount());

    RecheckTimer();
}

void APartyLobbyGameMode::OnPlayerButtonReleased(int32 PlayerIndex)
{
    if (!bButtonHeld.IsValidIndex(PlayerIndex)) { return; }

    bButtonHeld[PlayerIndex] = false;

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyLobbyGameMode: player %d released — %d held."),
        PlayerIndex + 1, GetActiveHeldCount());

    RecheckTimer();
}

void APartyLobbyGameMode::OnMainHold()
{
    // Hold = restart: travel to Main which resets and redirects back to Lobby.
    UE_LOG(LogPartyButtons, Log, TEXT("APartyLobbyGameMode: restarting via Main."));
    GetWorldTimerManager().ClearTimer(CountdownTimer);
    bCountingDown = false;
    TravelToPhase(EPartyPhase::Main);
}

// ---- Timer management ------------------------------------------------------

int32 APartyLobbyGameMode::GetActiveHeldCount() const
{
    int32 Count = 0;
    for (bool b : bButtonHeld) { if (b) { ++Count; } }
    return Count;
}

void APartyLobbyGameMode::RecheckTimer()
{
    const int32 Held = GetActiveHeldCount();

    if (Held >= MIN_PLAYERS && !bCountingDown)
    {
        // Enough players — start the countdown.
        bCountingDown    = true;
        CountdownEndTime = GetWorld()->GetTimeSeconds() + LOBBY_COUNTDOWN;

        GetWorldTimerManager().SetTimer(
            CountdownTimer,
            this,
            &APartyLobbyGameMode::OnCountdownComplete,
            LOBBY_COUNTDOWN,
            /*bLooping=*/false);

        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyLobbyGameMode: %d players held — countdown started (%.1fs)."),
            Held, (float)LOBBY_COUNTDOWN);
    }
    else if (Held < MIN_PLAYERS && bCountingDown)
    {
        // Too few players — reset the countdown.
        GetWorldTimerManager().ClearTimer(CountdownTimer);
        bCountingDown = false;

        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyLobbyGameMode: dropped to %d held — countdown reset."), Held);
    }
}

void APartyLobbyGameMode::OnCountdownComplete()
{
    bCountingDown = false;

    const int32 Held = GetActiveHeldCount();
    if (Held < MIN_PLAYERS)
    {
        // Shouldn't happen (RecheckTimer would have cleared the timer), but guard anyway.
        UE_LOG(LogPartyButtons, Warning,
            TEXT("APartyLobbyGameMode: countdown complete but only %d held — ignoring."), Held);
        return;
    }

    // Register exactly the players who are still holding their button.
    if (UPartySessionSubsystem* S = Session())
    {
        for (int32 i = 0; i < bButtonHeld.Num(); i++)
        {
            if (bButtonHeld[i])
            {
                S->RegisterPlayer(i);
            }
        }
    }

    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyLobbyGameMode: %d player(s) registered — going to LevelSelect."), Held);

    TravelToPhase(EPartyPhase::LevelSelect);
}
