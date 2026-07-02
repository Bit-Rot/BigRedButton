#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartySettingsGameMode.generated.h"

/**
 * APartySettingsGameMode
 *
 * Used by L_Settings. Debug/stub settings screen.
 *
 * Options shown:
 *   0: Games Per Session (current value: N; tap to cycle through preset values)
 *   1: Keyboard Emulation (on/off; tap to toggle)
 *
 * Controls:
 *   Any player button  → cycle the selection cursor.
 *   Main button tap    → change the value of the highlighted setting.
 *   Main button hold   → back to MainMenu.
 */
UCLASS()
class PARTYBUTTONS_API APartySettingsGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    APartySettingsGameMode();

    virtual FString GetHudTitle()       const override { return TEXT("Settings"); }
    virtual int32   GetSelectionIndex() const override { return SelectedOption; }

    /** Number of configurable options shown in the HUD. */
    int32 GetOptionCount() const { return NUM_OPTIONS; }

    /** Human-readable current value string for option i (for the HUD). */
    FString GetOptionLabel(int32 OptionIndex) const;
    FString GetOptionValue(int32 OptionIndex) const;

protected:
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnMainTap()  override;
    virtual void OnMainHold() override;

private:
    int32 SelectedOption = 0;
    static constexpr int32 NUM_OPTIONS = 2;

    // Preset values for Games Per Session.
    static const int32 GamesPerSessionPresets[];
    int32 GamesPerSessionPresetIndex = 0; // index into GamesPerSessionPresets

    void ActivateSelectedOption();
};
