#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "OctoOdyssey/OctoTuning.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoOdyssey/OctoScores.h"
#include "OctoGameMode.generated.h"

class AOctoPawn;
class AOctoCamera;
class AOctoViewPoint;

/**
 * AOctoGameMode
 *
 * GameMode for L_OctoOdyssey — a QWOP-style co-op physics game where up to 8
 * players each control one arm of a single shared AOctoPawn (button i extends
 * arm i, releasing it retracts), racing a clock to a goal flag.
 *
 * ---- What this class IS, and what it deliberately stopped being --------------
 *
 * OctoOdyssey used to be roster slot 2 of the 16-minigame party session: an
 * APartyMinigameGameMode subclass that resolved itself from the session roster,
 * ran the shared tutorial/countdown intro, and travelled back into
 * LevelSelect/Results when it ended. It is now a self-contained GAME — its own
 * menu, its own two courses, its own scoreboard — and derives straight from
 * APartyGameModeBase instead. Three things forced that and are worth stating,
 * because each one is a reason not to "just" put it back:
 *
 *   1. APartyMinigameGameMode::OnPlayerButton is FINAL and swallows every press
 *      that isn't during live gameplay. A main menu and a name-entry screen both
 *      live outside live gameplay, so under that base they could not be typed
 *      into at all.
 *   2. DeclareWinner/DeclareNoContest record a win against a session and travel
 *      away. This game has no session and nowhere to travel to: it is one level.
 *   3. The intro overlay assumes it runs once, at BeginPlay. Here a run starts
 *      when a player picks it out of a menu, any number of times per launch.
 *
 * THERE IS NO MAP TRAVEL IN THIS GAME. Menu island, normal course and hard
 * course are three places in ONE level (stacked along -X — see EOctoCourse), and
 * moving between them is a camera blend, not an OpenLevel. That is what makes
 * the flow instant and what lets the scoreboard live in memory between runs.
 *
 * ---- Flow --------------------------------------------------------------------
 *
 *   MainMenu    Fixed camera on the menu island (the placed AOctoViewPoint), no
 *               octopus in the world. Any player button cycles the three options;
 *               the main button taps to choose.
 *   Playing     The chosen course's AOctoSpawnPoint spawns an octopus, an
 *               AOctoCamera follows it, the view blends across, and the clock
 *               runs. Deliberately no countdown and no tutorial: menu -> spawn ->
 *               play, straight through.
 *   ScoreEntry  Reaching that course's flag stops the clock, tears the octopus
 *               down, blends back to the island and shows the table with the
 *               player's row editable IN PLACE. Player i owns letter i; a tap
 *               advances it, a hold repeats. The main button held for a second
 *               commits (see AOctoPlayerController for that second).
 *   ScoreView   Both tables side by side, read-only. Main button returns.
 *
 * ---- Location comes from the level, not from code ----------------------------
 *
 * Every position that decides WHERE something happens is an actor the user can
 * drag in the editor: AOctoSpawnPoint (per course), AOctoGoalFlag (per course),
 * AOctoViewPoint (the menu shot). A course's spawn point X is also the play
 * plane for that course — the octopus is pinned to it and the camera frames it —
 * so the two can never disagree (see AOctoCamera::SetPlayPlaneX).
 *
 * ---- Dev tuning menu (Tab, non-Shipping only) --------------------------------
 *
 * Unchanged, and still the reference implementation of the generic dev menu on
 * APartyGameModeBase: Tab lists every field of FOctoTuning, Up/Down select,
 * Left/Right change, the mouse can drag the sliders, and gameplay keeps RUNNING
 * behind the dialog because a live preview of a damping value is worthless if the
 * octopus is frozen. Values that can't be applied live (geometry) take effect on
 * ACCEPT, which now RESPAWNS THE OCTOPUS rather than reloading the level — same
 * result, no travel. Every exit writes Config/OctoTuning.ini.
 *
 * AI is deliberately NOT implemented. Every arm drives the SAME shared body, so
 * an AI holding arms that fight the humans' intent is actively worse than no AI
 * at all.
 */
UCLASS()
class PARTYBUTTONS_API AOctoGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    AOctoGameMode();

    virtual FString GetHudTitle() const override { return TEXT("OCTO ODYSSEY"); }
    virtual FString GetHudSubtitle() const override;

    // ---- State, read by AOctoHUD ---------------------------------------------
    //
    // These live here rather than as virtuals on APartyGameModeBase because they
    // are Octo-specific: AOctoHUD casts to this class, so the shared base does not
    // grow a menu-selection or scoreboard concept it has no other use for.

    EOctoFlowState GetFlowState()   const { return FlowState; }
    EOctoCourse    GetActiveCourse() const { return ActiveCourse; }

    /** Highlighted menu row, 0..NumMenuOptions-1. Also drives APartyGameModeBase::GetSelectionIndex. */
    virtual int32 GetSelectionIndex() const override { return MenuSelection; }

    static constexpr int32 NumMenuOptions = 3;

    /** Menu row labels, in order. Owned here so the HUD never re-declares them. */
    static const TCHAR* GetMenuOptionLabel(int32 OptionIndex);

    /**
     * Seconds on the clock: counting up while Playing, frozen at the finish time
     * on the score screens, 0 in the menu.
     */
    float GetRunSeconds() const;

    /** Where the just-finished run lands in its table, or INDEX_NONE if it missed the top ten. */
    int32 GetPendingRank() const { return PendingRank; }

    /** The eight-character name being typed. Always OctoScores::NameLength long. */
    const FString& GetPendingName() const { return PendingName; }

    /** A course's live table, or an empty one if the score subsystem is unavailable. */
    const TArray<FOctoScoreEntry>& GetScoreTable(EOctoCourse Course) const;

    // ---- Dev tuning menu, consumed by APartyFlowHUD::DrawDevMenu -------------
    virtual bool                     GetDevMenuOpen() const override { return bDevMenuOpen; }
    virtual TArray<FPartyDevMenuRow> GetDevMenuRows() const override;
    virtual int32                    GetDevMenuSelection() const override { return DevMenuSelection; }
    virtual FString                  GetDevMenuTitle() const override { return TEXT("OCTO TUNING"); }
    virtual void                     SetDevMenuSelection(int32 RowIndex) override;
    virtual void                     SetDevMenuRowNormalized(int32 RowIndex, float Alpha) override;
    virtual void                     ActivateDevMenuRow(int32 RowIndex) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

    // ---- Input ---------------------------------------------------------------
    //
    // All four route through the FlowState switch. Nothing here is final and
    // nothing is gated on an intro phase — that gating is precisely what this
    // class left behind (see the class comment).

    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override;
    virtual void OnMainTap() override;
    virtual void OnMainHold() override;

    /** Tab: open/close the tuning dialog. Never fires in Shipping — see APartyInputController::OnDevMenu. */
    virtual void OnDevMenuToggle() override;

    /** Up/Down move the menu selection while it's open; otherwise they do nothing here. */
    virtual void OnDevIncrement() override;
    virtual void OnDevDecrement() override;

    /** Left/Right nudge the selected value and push the result live. */
    virtual void OnDevLeft() override;
    virtual void OnDevRight() override;

    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    TSubclassOf<AOctoPawn> OctoPawnClass;

    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    TSubclassOf<AOctoCamera> OctoCameraClass;

    /**
     * Course-level tunables, pulled from UOctoTuningSubsystem in BeginPlay and
     * edited in place by the dev menu.
     */
    FOctoTuning Tuning;

    /** Used only if the chosen course has no AOctoSpawnPoint placed. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FVector FallbackSpawnLocation = FVector(0.f, 300.f, 200.f);

    /** Seconds the view takes to cross between the menu camera and a course camera. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    float ViewBlendSeconds = 1.0f;

    /** How long a name-entry button must be held before its letter starts auto-advancing. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Name Entry")
    float NameHoldDelaySeconds = 0.4f;

    /** Letters per second while held. Fast enough to cross the alphabet in ~5s, slow enough to stop on one. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Name Entry")
    float NameHoldRepeatHz = 7.f;

    /** Spawn a Movable directional light if the level has none — keeps a half-built map visible. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    bool bSpawnFallbackLight = true;

private:
    // ---- Flow ----------------------------------------------------------------

    /** Spawn the octopus and its camera on Course, blend the view over, start the clock. */
    void EnterCourse(EOctoCourse Course);

    /** Destroy the octopus and its camera, blend back to the menu view point. */
    void LeaveCourse();

    /** Blend to the menu island and show the menu. Shared exit from every other state. */
    void ReturnToMenu();

    /** Bound to every AOctoGoalFlag::OnReached. */
    void HandleGoalReached(EOctoCourse Course);

    /** Commit PendingName into the table for ActiveCourse (if it qualified) and go back to the menu. */
    void CommitPendingScore();

    /**
     * Rebuild the octopus in place, for tuning that can only be applied at spawn
     * (geometry — see AOctoPawn's class comment). Replaces the old ReloadCourse():
     * same effect, no OpenLevel, and the scoreboard survives.
     */
    void RestartRun();

    /** Act on the highlighted menu row. */
    void ActivateMenuOption();

    // ---- Level queries -------------------------------------------------------

    AOctoViewPoint* FindMenuViewPoint() const;

    /** Spawn location for Course, X included — that X is the course's play plane. */
    FVector FindSpawnLocation(EOctoCourse Course) const;

    /** Bind EVERY goal flag, not just the first: there is one per course. */
    void BindGoalFlags();

    void MaybeSpawnFallbackLight();

    /** Point the view at Target, blending unless bImmediate. Safe with a null Target. */
    void SetViewTo(AActor* Target, bool bImmediate);

    // ---- Name entry ----------------------------------------------------------

    /** Advance letter LetterIndex by one glyph. No-op unless the run actually placed. */
    void CycleNameLetter(int32 LetterIndex);

    /** Per-button hold state, so a held button auto-advances its own letter. */
    struct FLetterHold
    {
        float Elapsed      = 0.f;
        float NextRepeatAt = 0.f;
    };

    void TickNameEntry(float DeltaSeconds);

    // ---- Dev tuning menu ------------------------------------------------------

    void PushLiveTuning();
    void MoveDevMenuSelection(int32 Delta);
    void NudgeDevMenuSelection(int32 Direction);
    void SetDevMenuInputMode(bool bMenuOpen);
    void CloseDevMenu();
    void LoadAndApplyCourseTuning();

    /**
     * Row indices are menu-space (headers included) while OctoTuning::GetParams
     * is param-space — this maps one to the other, returning INDEX_NONE for
     * headers and action rows.
     */
    int32 DevMenuRowToParamIndex(int32 RowIndex) const;

    /** Menu-space index of the Accept row. Reset is the one after it, Cancel after that. */
    int32 GetDevMenuAcceptRowIndex() const;

    // ---- State ---------------------------------------------------------------

    UPROPERTY()
    TObjectPtr<AOctoPawn> Octo;

    UPROPERTY()
    TObjectPtr<AOctoCamera> Camera;

    UPROPERTY()
    TObjectPtr<AOctoViewPoint> MenuViewPoint;

    EOctoFlowState FlowState    = EOctoFlowState::MainMenu;
    EOctoCourse    ActiveCourse = EOctoCourse::Normal;
    int32          MenuSelection = 0;

    /** World time the current run started. Only meaningful while Playing. */
    float RunStartTime = 0.f;

    /** The finished run's time, held for the score screens. */
    float FinishedSeconds = 0.f;

    int32   PendingRank = INDEX_NONE;
    FString PendingName;

    TMap<int32, FLetterHold> LetterHolds;

    bool  bDevMenuOpen     = false;
    int32 DevMenuSelection = 0;
};
