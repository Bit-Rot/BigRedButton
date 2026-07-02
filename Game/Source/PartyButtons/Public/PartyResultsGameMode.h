#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartyResultsGameMode.generated.h"

/**
 * APartyResultsGameMode
 *
 * Used by L_Results. Displays the end-of-session leaderboard.
 * The HUD reads win tallies from UPartySessionSubsystem.
 *
 * Controls:
 *   Main button tap or hold → reset the session and return to MainMenu.
 *   (Player buttons are ignored on this screen — it's just a scoreboard.)
 */
UCLASS()
class PARTYBUTTONS_API APartyResultsGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    virtual FString GetHudTitle() const override { return TEXT("Results"); }

protected:
    virtual void OnMainTap()  override;
    virtual void OnMainHold() override;

private:
    void ReturnToMainMenu();
};
