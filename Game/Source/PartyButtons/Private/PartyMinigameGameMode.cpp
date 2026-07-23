#include "PartyMinigameGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"

void APartyMinigameGameMode::BeginPlay()
{
    Super::BeginPlay();

    bWinnerDeclared   = false;
    CurrentRosterIndex = INDEX_NONE;
    CurrentGameName   = TEXT("???");

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

void APartyMinigameGameMode::OnPlayerButton(int32 PlayerIndex)
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
