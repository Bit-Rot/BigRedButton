#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PartyButtonsHUD.generated.h"

/**
 * Visual demo HUD: draws a 4x4 grid of 16 button indicators on screen.
 * Each cell flashes for FLASH_DURATION seconds when the corresponding
 * button fires. Binds to APartyInputController::OnButtonPressed on BeginPlay.
 *
 * Pure C++ canvas drawing — no UMG assets required.
 */
UCLASS()
class PARTYBUTTONS_API APartyButtonsHUD : public AHUD
{
    GENERATED_BODY()

public:
    APartyButtonsHUD();

protected:
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

private:
    /** Timestamp (GetWorld()->GetTimeSeconds()) of the last press for each button. */
    float ButtonLastPressed[16];

    void HandleButtonPressed(int32 PlayerIndex);

    static constexpr int32 NUM_BUTTONS   = 16;
    static constexpr int32 GRID_COLS     = 4;
    static constexpr int32 GRID_ROWS     = 4;
    static constexpr float CELL_SIZE     = 80.f;
    static constexpr float CELL_MARGIN   = 10.f;
    static constexpr float FLASH_DURATION = 0.3f;
};
