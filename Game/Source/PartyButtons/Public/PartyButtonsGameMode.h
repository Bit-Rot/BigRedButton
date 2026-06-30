#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PartyButtonsGameMode.generated.h"

/**
 * Minimal game mode for the PartyButtons test project.
 * Sets APartyInputController as the player controller and
 * APartyButtonsHUD as the HUD. No default pawn.
 */
UCLASS()
class PARTYBUTTONS_API APartyButtonsGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APartyButtonsGameMode();
};
