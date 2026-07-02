#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartyMainMenuGameMode.generated.h"

/**
 * APartyMainMenuGameMode
 *
 * Used by L_MainMenu. Presents two options: Play and Settings.
 *
 * Controls:
 *   Any player button  → cycle the selection cursor (wraps).
 *   Main button tap    → activate the selected option.
 *   Main button hold   → no-op (already at the top level).
 */
UCLASS()
class PARTYBUTTONS_API APartyMainMenuGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    APartyMainMenuGameMode();

    virtual FString GetHudTitle()     const override { return TEXT("Party Buttons"); }
    virtual int32   GetSelectionIndex() const override { return SelectedOption; }

protected:
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnMainTap()  override;
    virtual void OnMainHold() override {} // no-op at top level

private:
    // 0 = Play, 1 = Settings
    int32 SelectedOption = 0;
    static constexpr int32 NUM_OPTIONS = 2;
};
