#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PartyTypes.h"
#include "PartyFlowHUD.generated.h"

class UPartySessionSubsystem;
class APartyGameModeBase;
struct FPartySessionState;

/**
 * APartyFlowHUD
 *
 * Single canvas HUD for all PartyButtons phases. Switches drawing logic based on
 * the current EPartyPhase queried from UPartySessionSubsystem.
 *
 * Phase-specific data is queried via virtual accessors on APartyGameModeBase (e.g.
 * GetHighlightTile, GetCountdownRemaining, GetSelectionIndex), so no concrete
 * GameMode headers are required here — only the base.
 *
 * Pitfall guard: during map travel, the auth GameMode and the subsystem may both be
 * null for one frame. All getters are null-guarded; DrawHUD silently no-ops in that case.
 *
 * Reuses the 4×4 grid layout from APartyButtonsHUD (same constants, same DrawGrid helper).
 * APartyButtonsHUD is kept as-is for L_ButtonTest.
 */
UCLASS()
class PARTYBUTTONS_API APartyFlowHUD : public AHUD
{
    GENERATED_BODY()

protected:
    virtual void DrawHUD() override;

private:
    // ---- Helpers -------------------------------------------------------

    UPartySessionSubsystem* GetSession()  const;
    APartyGameModeBase*     GetFlowGM()   const;

    // ---- Per-phase draw functions (each null-guards its own data needs) ---

    void DrawMainMenu(APartyGameModeBase* GM);
    void DrawSettings(APartyGameModeBase* GM);
    void DrawLobby(APartyGameModeBase* GM);
    void DrawLevelSelect(APartyGameModeBase* GM, const FPartySessionState& State);
    void DrawMinigame(APartyGameModeBase* GM);
    void DrawResults(const FPartySessionState& State);

    /**
     * Draw the 4×4 grid of tiles.
     * IsLit(i) controls the cell background color.
     * HighlightIndex (if >= 0) draws a brighter overlay on that tile.
     * Label(i) is the text drawn inside each cell (or empty string).
     */
    void DrawButtonGrid(
        TFunctionRef<bool(int32)>    IsLit,
        TFunctionRef<FString(int32)> GetLabel,
        int32                        HighlightIndex = INDEX_NONE);

    void DrawCenteredText(const FString& Text, float Y, FLinearColor Color, float Scale = 1.0f);

    // Tracks last phase that was logged so we only log on phase transitions.
    mutable EPartyPhase LastLoggedPhase = EPartyPhase::Main;

    // ---- Shared grid constants (match APartyButtonsHUD) ----------------

    static constexpr int32  GRID_COLS   = 4;
    static constexpr int32  GRID_ROWS   = 4;
    static constexpr float  CELL_SIZE   = 80.f;
    static constexpr float  CELL_MARGIN = 10.f;
    static constexpr float  GRID_START_X = 40.f;
    static constexpr float  GRID_START_Y = 190.f; // below title + subtitle rows
};
