#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartyMainGameMode.generated.h"

/**
 * APartyMainGameMode
 *
 * Used by L_Main — the startup map (GameDefaultMap / GlobalDefaultGameMode).
 * Immediately resets the session and redirects to the MainMenu on the next tick.
 * No player input is expected on this map; it is a transparent init/redirect step.
 */
UCLASS()
class PARTYBUTTONS_API APartyMainGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    virtual FString GetHudTitle() const override { return TEXT("Starting..."); }

protected:
    virtual void BeginPlay() override;
};
