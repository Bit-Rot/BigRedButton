#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PartyTypes.h"
#include "PartyGameModeBase.generated.h"

class APartyInputController;
class UPartySessionSubsystem;

/**
 * APartyGameModeBase
 *
 * Abstract base for all PartyButtons phase GameModes.
 *
 * Responsibilities:
 *   - Wires the PlayerController input delegates (OnButtonPressed, OnMainButtonTapped,
 *     OnMainButtonHeld) to virtual handlers that subclasses override per-phase.
 *   - Owns the TravelToPhase() / TravelToGame() helpers that route map travel through
 *     the central PartyFlowRouter (always deferred one tick to avoid in-BeginPlay travel).
 *   - Exposes virtual HUD-facing accessors (GetHudTitle, GetHighlightTile, etc.) so
 *     APartyFlowHUD can query phase state without casting to concrete GameMode types.
 *
 * Binding strategy:
 *   Delegates are bound in OnPostLogin() — the guaranteed hook where the PC exists.
 *   Binding in BeginPlay() + GetFirstPlayerController() is order-dependent and fragile.
 *
 * Travel pitfall reminder:
 *   NEVER call OpenLevel synchronously inside BeginPlay. Always defer via
 *   SetTimerForNextTick(). Both TravelToPhase and TravelToGame do this automatically.
 */
UCLASS(Abstract)
class PARTYBUTTONS_API APartyGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    APartyGameModeBase();

    // ---- HUD-facing virtuals (defaults are safe no-ops / sentinel values) -----

    /** Title string shown by APartyFlowHUD for this phase. */
    virtual FString GetHudTitle() const;

    /**
     * Index of the highlighted tile in the 4×4 grid (used by LevelSelect HUD).
     * Returns INDEX_NONE when not applicable.
     */
    virtual int32 GetHighlightTile() const { return INDEX_NONE; }

    /**
     * Remaining seconds in the lobby countdown, or -1 if not counting down.
     */
    virtual float GetCountdownRemaining() const { return -1.0f; }

    /**
     * True if player i has registered (joined) this session. Used by the Lobby HUD.
     */
    virtual bool IsTileJoined(int32 PlayerIndex) const;

    /**
     * Index of the currently highlighted menu option, or INDEX_NONE.
     * Used by MainMenu and Settings HUDs.
     */
    virtual int32 GetSelectionIndex() const { return INDEX_NONE; }

protected:
    // ---- Phase handler virtuals (subclasses override what they care about) ----

    /** Called when any of the 16 player buttons is pressed. */
    virtual void OnPlayerButton(int32 PlayerIndex) {}

    /** Called when any of the 16 player buttons is released. */
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) {}

    /** Called when the main button is tapped (short press — activate/confirm). */
    virtual void OnMainTap() {}

    /** Called when the main button is held long enough (go back/up). */
    virtual void OnMainHold() {}

    // ---- Travel helpers -------------------------------------------------------

    /** Subsystem convenience — returns nullptr if not in a valid game world. */
    UPartySessionSubsystem* Session() const;

    /**
     * Write the new phase to the subsystem, then schedule an OpenLevel call
     * for the next tick. The phase is written BEFORE travel so the destination
     * HUD's first DrawHUD reads the correct phase immediately.
     */
    void TravelToPhase(EPartyPhase Phase);

    /**
     * Set the current game in the subsystem, set phase to Minigame, then
     * schedule travel to the roster entry's map with the Minigame GameMode.
     */
    void TravelToGame(int32 RosterIndex);

    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

private:
    // Trampoline handlers that call the virtual On* methods.
    void HandlePlayerButtonDelegate(int32 PlayerIndex)         { OnPlayerButton(PlayerIndex); }
    void HandlePlayerButtonReleasedDelegate(int32 PlayerIndex) { OnPlayerButtonReleased(PlayerIndex); }
    void HandleMainTapDelegate()                               { OnMainTap(); }
    void HandleMainHoldDelegate()                              { OnMainHold(); }

    // Stored so we can remove bindings if needed.
    APartyInputController* BoundController = nullptr;
};
