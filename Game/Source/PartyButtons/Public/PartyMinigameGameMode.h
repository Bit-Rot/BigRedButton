#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartyMinigameGameMode.generated.h"

class USoundBase;

/**
 * APartyMinigameGameMode
 *
 * Shared GameMode used by ALL 16 minigame maps (L_GameA..L_GameP).
 * Resolves which game it is by comparing the current map name against the roster.
 *
 * Rules:
 *   - The FIRST registered player to press their button wins.
 *   - Win is recorded, game count advances.
 *   - If GamesPerSession reached → Results. Otherwise → back to LevelSelect.
 *   - A guard flag (bWinnerDeclared) prevents double-fire on the same frame.
 *
 * Intro overlay (Tutorial → Discovery → Playing):
 *   EVERY minigame gets a tutorial dialog, a countdown, and only THEN live
 *   gameplay — no game has to opt in, and no game can accidentally leave a
 *   full-screen help overlay pinned over itself for the whole round (which is
 *   exactly what happened before this machine lived here). The sequence is:
 *
 *     Tutorial   dialog over the frozen-but-visible level. Dismissed by
 *                whichever comes first: TutorialMaxSeconds elapsing, or every
 *                human participant holding their button (the ready check).
 *     Discovery  "which one is me?" — beep....beep....BOOOP over the level,
 *                with GetPawnMarkers() labelling each pawn for games that
 *                provide markers. Games that return none (the default) simply
 *                get the countdown.
 *     Playing    gameplay is live, nothing is drawn over it.
 *
 *   Subclasses do NOT override OnPlayerButton — it is final and owns the gate.
 *   Override OnGameplayButton/OnGameplayButtonReleased instead; they are only
 *   ever called during Playing, so a game can never react to a press that was
 *   meant to dismiss the tutorial.
 *
 * PIE pitfall:
 *   GetWorld()->GetMapName() returns "UEDPIE_0_L_GameA" in PIE.
 *   We strip the PIE prefix via UWorld::RemovePIEPrefix before the roster lookup.
 *
 * HUD: displays the game's zany DisplayName in the center of the screen.
 */
UCLASS()
class PARTYBUTTONS_API APartyMinigameGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    APartyMinigameGameMode();

    virtual FString GetHudTitle() const override;

    // ---- Intro overlay, consumed by APartyFlowHUD::DrawMinigame ---------------
    virtual EPartyOverlayPhase GetOverlayPhase() const override { return IntroPhase; }
    virtual float              GetTutorialRemainingSeconds() const override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * Owns the intro gate: records held buttons, runs the tutorial ready check,
     * and forwards to OnGameplayButton only once the round is live. Final —
     * override OnGameplayButton instead, so no subclass can bypass the gate.
     */
    virtual void OnPlayerButton(int32 PlayerIndex) override final;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override final;

    /**
     * A button press during live gameplay. This is the per-game override point
     * that OnPlayerButton used to be. The default implements the shared
     * "first registered player to press wins" rule.
     */
    virtual void OnGameplayButton(int32 PlayerIndex);

    /** A button release during live gameplay. Empty default. */
    virtual void OnGameplayButtonReleased(int32 PlayerIndex) {}

    /**
     * Record PlayerIndex's win, advance the session, and travel to Results
     * (session complete) or back to LevelSelect. Shared by every minigame's
     * win path — subclasses with different win conditions (e.g. last player
     * standing) call this once they've determined the winner, instead of
     * duplicating the record/advance/travel sequence.
     */
    virtual void DeclareWinner(int32 PlayerIndex);

    /**
     * Advance the session with no win recorded (e.g. a mutual-death draw),
     * then travel exactly like DeclareWinner. Guarded the same way.
     */
    virtual void DeclareNoContest();

    /**
     * Per-frame hook for AI-controlled players (dev-only testing aid — see
     * FPartySessionState::NumAIPlayers, adjustable via the Lobby's Up/Down
     * control). Only called during Playing, so AI stays silent through the
     * intro for free. Empty default: for the 15 minigames that don't override
     * this, AI simply does nothing — the "AI does nothing if unimplemented"
     * tenet falls out of this default automatically, no per-game opt-out needed.
     * Overrides MUST drive AI through the same OnPlayerButton/OnPlayerButtonReleased
     * entry points real input uses (never touch a pawn/actor directly) so AI
     * can never do something a real player couldn't.
     */
    virtual void TickAI(float DeltaSeconds) {}

    // ---- Intro overlay hooks (override per game) -----------------------------

    /**
     * Stop/start whatever "gameplay running" means for this game, so the level
     * is visible but static behind the tutorial dialog. Called with true on the
     * first tick (see Tick — NOT BeginPlay, which runs before subclasses have
     * spawned anything to freeze) and with false on the BOOOP.
     *
     * Deliberately a hook rather than shared code: a tick-driven kinematic pawn
     * freezes with SetActorTickEnabled, but a physics-simulating one would keep
     * falling under gravity and needs its simulation stopped instead.
     */
    virtual void SetGameplayFrozen(bool bFrozen) {}

    /**
     * Declare that PlayerIndex takes part this round. Non-AI participants form
     * the set the tutorial's "all humans holding" ready check waits on — call
     * this for every participant as they're spawned/assigned, or the tutorial
     * can only ever be dismissed by its timer.
     */
    void RegisterParticipant(int32 PlayerIndex, bool bIsAI);

    /**
     * Should a button already held when the round goes live be replayed as a
     * fresh press into OnGameplayButton?
     *
     * Default FALSE, and that default is load-bearing: under the base
     * "first press wins" rule, replaying would hand the win to whoever held
     * their button to skip the tutorial. Games where holding is a charge/aim
     * action (see APartyArenaGameMode) opt in so pre-charging isn't punished.
     */
    virtual bool ShouldReplayHeldButtonsOnStart() const { return false; }

    /** Called at the end of EnterPlaying, after the held-button replay. */
    virtual void OnGameplayStarted() {}

    /** True once the countdown has finished and gameplay is live. */
    bool IsPlaying() const { return IntroPhase == EPartyOverlayPhase::Playing; }

    /** Participants currently holding their button — read by GetPawnMarkers overrides for the tint. */
    const TSet<int32>& GetHeldButtons() const { return HeldButtons; }

    /** Roster index of the current game, resolved in BeginPlay. */
    int32 CurrentRosterIndex = INDEX_NONE;

    /** Human-readable name cached in BeginPlay for the HUD. */
    FString CurrentGameName;

    /** True once a winner (or no-contest) has been declared; prevents double-fire. */
    bool bWinnerDeclared = false;

    // ---- Intro overlay tunables ----------------------------------------------

    /** Tutorial auto-dismiss deadline. The ready check can dismiss it sooner. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyMinigame|Intro")
    float TutorialMaxSeconds = 5.f;

    /** Countdown one-shot for the two lead-in beeps. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyMinigame|Intro")
    TObjectPtr<USoundBase> BeepSound;

    /** Countdown one-shot for the final "BOOOP" — game starts as this plays. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyMinigame|Intro")
    TObjectPtr<USoundBase> BoopSound;

private:
    /** Shared tail of DeclareWinner/DeclareNoContest: advance + travel. */
    void AdvanceAndTravel();

    void EnterDiscovery();
    void EnterPlaying();
    void PlayCountdownBeat(bool bIsBoop);

    /** False when no humans are registered — then only the timer dismisses the tutorial. */
    bool AllHumansHolding() const;

    // ---- Intro overlay state -------------------------------------------------
    EPartyOverlayPhase IntroPhase     = EPartyOverlayPhase::Tutorial;
    float              IntroElapsed   = 0.f;
    TSet<int32>        HeldButtons;       // any participant currently holding — ready check + discovery tint
    TSet<int32>        HumanParticipants; // the non-AI subset, filled by RegisterParticipant
    int32              NextCountdownBeat = 0;

    /** Guards the one-shot deferred freeze in Tick. */
    bool               bIntroFreezeApplied = false;
};
