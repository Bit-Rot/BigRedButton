#include "PartyMainMenuGameMode.h"
#include "PartyButtons.h"

APartyMainMenuGameMode::APartyMainMenuGameMode()
{
    SelectedOption = 0;
}

void APartyMainMenuGameMode::OnPlayerButton(int32 PlayerIndex)
{
    // Any player press cycles the cursor.
    SelectedOption = (SelectedOption + 1) % NUM_OPTIONS;
    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyMainMenuGameMode: selection -> %d (player %d pressed)"),
        SelectedOption, PlayerIndex + 1);
}

void APartyMainMenuGameMode::OnMainTap()
{
    switch (SelectedOption)
    {
    case 0: // Play
        UE_LOG(LogPartyButtons, Log, TEXT("APartyMainMenuGameMode: Play selected -> Lobby."));
        TravelToPhase(EPartyPhase::Lobby);
        break;
    case 1: // Settings
        UE_LOG(LogPartyButtons, Log, TEXT("APartyMainMenuGameMode: Settings selected -> Settings."));
        TravelToPhase(EPartyPhase::Settings);
        break;
    default:
        break;
    }
}
