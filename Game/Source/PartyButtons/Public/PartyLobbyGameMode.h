#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "Engine/TimerHandle.h"
#include "PartyLobbyGameMode.generated.h"

/**
 * APartyLobbyGameMode
 *
 * Used by L_Lobby. Players press their button to register for the session.
 *
 * Rules:
 *   - When MIN_PLAYERS humans are simultaneously holding, the countdown starts.
 *   - Any player pressing their button during the window registers them.
 *   - At countdown expiry, all registered players advance to LevelSelect.
 *   - AI slots fill in downstream but never gate the countdown — a solo dev
 *     with AI turned up still has to physically hold two buttons to start.
 *   - Main button hold → back to MainMenu.
 *
 * HUD: 4×4 grid with registered players lit; countdown remaining shown.
 */
UCLASS()
class PARTYBUTTONS_API APartyLobbyGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    APartyLobbyGameMode();

    virtual FString GetHudTitle()           const override { return TEXT("Lobby"); }
    virtual float   GetCountdownRemaining() const override;
    virtual bool    IsTileJoined(int32 i)   const override;

    /**
     * True if slot i is currently AI-controlled, computed against the LIVE
     * bButtonHeld array (not RegisteredPlayers — registration hasn't happened
     * yet during the countdown). See FPartySessionState::ComputeAISlots.
     */
    virtual bool IsTileAI(int32 i) const override;

protected:
    virtual void BeginPlay() override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override;
    virtual void OnMainHold() override;

    /** Dev-only Up/Down: adjust the AI player count. Does NOT affect the start countdown. */
    virtual void OnDevIncrement() override;
    virtual void OnDevDecrement() override;

private:
    void OnCountdownComplete();

    /** Recheck timer state after any press/release: start if humans held >= MIN_PLAYERS, stop if below. */
    void RecheckTimer();

    /** Count of buttons currently held down. */
    int32 GetActiveHeldCount() const;

    FTimerHandle CountdownTimer;
    bool  bCountingDown    = false;
    float CountdownEndTime = 0.0f;

    /** Per-button hold state: true while the physical button is depressed. */
    TArray<bool> bButtonHeld;

    static constexpr float LOBBY_COUNTDOWN  = 2.0f;
    static constexpr int32 MIN_PLAYERS      = 2;
};
