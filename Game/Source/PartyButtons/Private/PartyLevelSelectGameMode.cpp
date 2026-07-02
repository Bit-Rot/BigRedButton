#include "PartyLevelSelectGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"

void APartyLevelSelectGameMode::BeginPlay()
{
    Super::BeginPlay();
    HighlightIndex = 0;
    bConfirmed     = false;

    UE_LOG(LogPartyButtons, Log, TEXT("APartyLevelSelectGameMode: waiting for level selection."));
}

void APartyLevelSelectGameMode::OnPlayerButton(int32 PlayerIndex)
{
    if (bConfirmed) { return; }

    HighlightIndex = (HighlightIndex + 1) % NUM_TILES;

    if (UPartySessionSubsystem* S = Session())
    {
        const TArray<FPartyGameInfo>& Roster = S->GetRoster();
        const FString Name = Roster.IsValidIndex(HighlightIndex)
            ? Roster[HighlightIndex].DisplayName
            : TEXT("???");
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyLevelSelectGameMode: player %d advanced cursor → tile %d (%s)."),
            PlayerIndex + 1, HighlightIndex, *Name);
    }
}

void APartyLevelSelectGameMode::OnMainTap()
{
    if (bConfirmed) { return; }
    bConfirmed = true;

    if (UPartySessionSubsystem* S = Session())
    {
        const TArray<FPartyGameInfo>& Roster = S->GetRoster();
        const FString Name = Roster.IsValidIndex(HighlightIndex)
            ? Roster[HighlightIndex].DisplayName
            : TEXT("???");
        UE_LOG(LogPartyButtons, Log,
            TEXT("APartyLevelSelectGameMode: confirmed tile %d (%s)."),
            HighlightIndex, *Name);
    }

    TravelToGame(HighlightIndex);
}

void APartyLevelSelectGameMode::OnMainHold()
{
    if (bConfirmed) { return; }
    UE_LOG(LogPartyButtons, Log, TEXT("APartyLevelSelectGameMode: returning to lobby."));
    TravelToPhase(EPartyPhase::Lobby);
}
