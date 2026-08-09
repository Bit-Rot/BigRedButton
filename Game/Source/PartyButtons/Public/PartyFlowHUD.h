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

    // Intro-overlay draw paths for minigames that opt in via GetOverlayPhase.
    void DrawTutorialDialog(APartyGameModeBase* GM);
    void DrawPawnMarkers(APartyGameModeBase* GM);

    /**
     * Dev tuning menu (Tab). Drawn OVER whatever phase is active, from the rows
     * the GameMode supplies (APartyGameModeBase::GetDevMenuRows) — this HUD never
     * learns what any row means, which is what keeps it free of concrete GameMode
     * headers.
     *
     * Also owns the mouse: DrawHUD is the only per-frame place that knows the row
     * layout, so hit-testing lives here rather than being duplicated in the
     * GameMode. Clicks and drags are routed back through the GameMode's setters.
     */
    void DrawDevMenu(APartyGameModeBase* GM);

    /**
     * Draw the 4×4 grid of tiles.
     * IsLit(i) controls the cell background color (human/joined — green).
     * IsAI(i) controls the AI-filled color (dev-only testing aid — blue).
     * Color precedence: highlight > lit(human) > AI > idle, so a tile is
     * never drawn as more than one state.
     * HighlightIndex (if >= 0) draws a brighter overlay on that tile.
     * Label(i) is the text drawn inside each cell (or empty string).
     */
    void DrawButtonGrid(
        TFunctionRef<bool(int32)>    IsLit,
        TFunctionRef<bool(int32)>    IsAI,
        TFunctionRef<FString(int32)> GetLabel,
        int32                        HighlightIndex = INDEX_NONE);

    void DrawCenteredText(const FString& Text, float Y, FLinearColor Color, float Scale = 1.0f);

    // Tracks last phase that was logged so we only log on phase transitions.
    mutable EPartyPhase LastLoggedPhase = EPartyPhase::Main;

    // ---- Dev menu mouse state ------------------------------------------

    /** Row whose slider the mouse grabbed, held until the button comes back up. */
    int32 DevMenuDragRow = INDEX_NONE;

    /** Previous frame's left-button state, so a click fires once instead of every frame. */
    bool  bDevMenuMouseWasDown = false;

    // ---- Dev menu layout ------------------------------------------------

    static constexpr float DEV_ROW_HEIGHT   = 22.f;
    static constexpr float DEV_PANEL_PAD    = 16.f;
    static constexpr float DEV_HEADER_SPACE = 64.f;  // title + hint above the first row
    static constexpr float DEV_FOOTER_SPACE = 26.f;  // scroll indicator below the last row
    static constexpr float DEV_LABEL_WIDTH  = 250.f;
    static constexpr float DEV_VALUE_WIDTH  = 80.f;

    // ---- Shared grid constants (match APartyButtonsHUD) ----------------

    static constexpr int32  GRID_COLS   = 4;
    static constexpr int32  GRID_ROWS   = 4;
    static constexpr float  CELL_SIZE   = 80.f;
    static constexpr float  CELL_MARGIN = 10.f;
    static constexpr float  GRID_START_X = 40.f;
    static constexpr float  GRID_START_Y = 170.f; // below title + subtitle rows (Lobby now has 4 text rows above)
};
