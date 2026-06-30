#include "PartyButtonsGameMode.h"
#include "PartyButtonsHUD.h"
#include "PartyInputController.h"

APartyButtonsGameMode::APartyButtonsGameMode()
{
    PlayerControllerClass = APartyInputController::StaticClass();
    HUDClass = APartyButtonsHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
