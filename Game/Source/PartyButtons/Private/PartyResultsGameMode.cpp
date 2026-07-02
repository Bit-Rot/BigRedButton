#include "PartyResultsGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"

void APartyResultsGameMode::OnMainTap()
{
    ReturnToMainMenu();
}

void APartyResultsGameMode::OnMainHold()
{
    ReturnToMainMenu();
}

void APartyResultsGameMode::ReturnToMainMenu()
{
    UE_LOG(LogPartyButtons, Log, TEXT("APartyResultsGameMode: resetting session and returning to Lobby."));

    if (UPartySessionSubsystem* S = Session())
    {
        S->ResetSession();
    }

    TravelToPhase(EPartyPhase::Lobby);
}
