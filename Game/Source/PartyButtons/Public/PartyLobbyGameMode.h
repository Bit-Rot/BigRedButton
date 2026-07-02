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
 *   - When the FIRST button is pressed, a 1-second countdown timer starts.
 *   - Any player pressing their button during the window registers them.
 *   - At countdown expiry, all registered players advance to LevelSelect.
 *   - If nobody has registered when the timer would fire (edge case guard),
 *     the timer is restarted on the next press.
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

protected:
    virtual void BeginPlay() override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override;
    virtual void OnMainHold() override;

private:
    void OnCountdownComplete();

    /** Recheck timer state after any press/release: start if >=2 held, stop if <=1. */
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
