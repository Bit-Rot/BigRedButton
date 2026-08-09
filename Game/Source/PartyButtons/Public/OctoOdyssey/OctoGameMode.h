#pragma once

#include "CoreMinimal.h"
#include "PartyMinigameGameMode.h"
#include "OctoOdyssey/OctoTuning.h"
#include "OctoGameMode.generated.h"

class AOctoPawn;
class AOctoCamera;

/**
 * AOctoGameMode
 *
 * GameMode for L_GameC ("Octo Odyssey", roster slot 2) — a QWOP-style co-op
 * physics game. Up to 8 players each control one arm of a single shared
 * AOctoPawn; button i extends arm i, releasing it retracts. Reaching the
 * goal flag (AOctoGoalFlag, placed in the level) reloads L_GameC so the
 * group starts over — this game does NOT call DeclareWinner/DeclareNoContest
 * and never rejoins the LevelSelect/Results flow (see ReloadCourse's
 * comment; this is deliberate, matching the spec's "loop forever").
 *
 * Overrides the shared "first press wins" rule
 * (APartyMinigameGameMode::OnGameplayButton) entirely: every one of the first
 * 8 buttons always drives its arm, independent of Lobby registration
 * (bRequireRegistration defaults false) — with fewer than ~4 live arms the
 * course is uncompletable, and un-gating keeps L_GameC directly PIE-testable
 * via the dev keyboard mappings without going through the Lobby. Buttons
 * 8-15 are not used by this minigame.
 *
 * Setup (BeginPlay, after Super::BeginPlay() resolves the roster entry for
 * the HUD):
 *   1. Find an AOctoSpawnPoint (falling back to FallbackSpawnLocation if
 *      absent) and spawn one AOctoPawn there, X forced to PlayPlaneX.
 *   2. Spawn an AOctoCamera, follow the octopus, and set it as the view
 *      target — the project never possesses pawns (see APartyDuelPawn's
 *      class comment), so this is the only way the player sees anything.
 *   3. Find an AOctoGoalFlag and bind its OnReached delegate to
 *      ReloadCourse(). Warns (doesn't fail) if the level has none placed.
 *   4. Spawn a fallback Movable directional light if the level has none,
 *      so a half-built map is still visible.
 *
 * Dev tuning menu (Tab, non-Shipping only):
 *   This game mode is the reference implementation of the generic dev menu on
 *   APartyGameModeBase. Tab opens a dialog listing every field of FOctoTuning;
 *   Up/Down select, Left/Right change, and the mouse can drag the sliders.
 *   Gameplay deliberately keeps RUNNING behind the dialog — a live preview of a
 *   damping or grip value is worthless if the octopus is frozen — and every
 *   change that can be applied without rebuilding the pawn is pushed straight
 *   through AOctoPawn/AOctoCamera::ApplyLiveTuning. Geometry can't be
 *   (see AOctoPawn's class comment), so Accept dumps the tuning as JSON to the
 *   log and reloads the course; Cancel (and Tab) just close. Every exit writes
 *   Config/OctoTuning.ini, so a tuning pass survives both the reload and closing
 *   the editor — and RESET TO DEFAULTS clears that file when a stale override
 *   starts masking a C++ default.
 *
 * AI is deliberately NOT implemented (no TickAI override — the empty
 * default from APartyMinigameGameMode already IS "AI does nothing", no
 * opt-out needed). Reflex Rumble's AI fills an OPPOSING slot, so weak AI is
 * just a weak opponent; here every arm drives the SAME shared body, so an
 * AI holding arms that fight the humans' intent is actively worse than no
 * AI at all. A QWOP octopus also needs real locomotion planning (contact-
 * phase detection, gait sequencing) to be anything but noise — out of scope
 * for a first pass.
 */
UCLASS()
class PARTYBUTTONS_API AOctoGameMode : public APartyMinigameGameMode
{
    GENERATED_BODY()

public:
    AOctoGameMode();

    virtual FString GetHudSubtitle() const override;

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
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void OnGameplayButton(int32 PlayerIndex) override;
    virtual void OnGameplayButtonReleased(int32 PlayerIndex) override;
    // Deliberately no TickAI override — see class comment.

    /**
     * The octopus genuinely simulates physics, so the base's tick-suspension
     * idea doesn't apply — SetActorTickEnabled would stop the arms but let the
     * body keep falling. Stops the simulation instead (see AOctoPawn::SetPhysicsFrozen).
     */
    virtual void SetGameplayFrozen(bool bFrozen) override;

    /** Tab: open/close the tuning dialog. Never fires in Shipping — see APartyInputController::OnDevMenu. */
    virtual void OnDevMenuToggle() override;

    /** Up/Down move the menu selection while it's open; otherwise they do nothing here. */
    virtual void OnDevIncrement() override;
    virtual void OnDevDecrement() override;

    /** Left/Right nudge the selected value and push the result live. */
    virtual void OnDevLeft() override;
    virtual void OnDevRight() override;

    /** A button held through the countdown means "extend that arm" — honour it the instant play starts. */
    virtual bool ShouldReplayHeldButtonsOnStart() const override { return true; }

    // Deliberately no GetPawnMarkers override: the discovery phase labels one
    // pawn per player, and this game has ONE shared body for all eight. The
    // countdown still runs (it's the "GO!" signal) — just without labels.

    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    TSubclassOf<AOctoPawn> OctoPawnClass;

    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    TSubclassOf<AOctoCamera> OctoCameraClass;

    /**
     * Course-level tunables, pulled from UOctoTuningSubsystem in BeginPlay and
     * edited in place by the dev menu. The play plane lives in here rather than
     * as its own property because AOctoCamera needs the same number — they used
     * to be two independent copies that silently desynced.
     */
    FOctoTuning Tuning;

    /** Used only if no AOctoSpawnPoint is placed in the level. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FVector FallbackSpawnLocation = FVector(0.f, 300.f, 200.f);

    /** Used only if the roster lookup fails (e.g. L_GameC opened directly with no session). */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FName FallbackMapName = TEXT("L_GameC");

    /**
     * If true, OnGameplayButton/OnGameplayButtonReleased ignore unregistered
     * players (matching APartyMinigameGameMode's default gating). Defaults
     * false — see class comment for why this minigame un-gates deliberately.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    bool bRequireRegistration = false;

    /** Spawn a Movable directional light if the level has none — keeps a half-built map visible. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    bool bSpawnFallbackLight = true;

private:
    void SpawnOctopus();
    void SpawnCamera();
    void BindGoalFlag();
    void MaybeSpawnFallbackLight();

    /** Declares who the tutorial's "all humans holding" ready check waits on. */
    void RegisterArmParticipants();

    /** Bound to AOctoGoalFlag::OnReached. */
    void HandleGoalReached();

    /**
     * Reload L_GameC (or the roster-resolved map, if this instance is
     * running as part of a session) with THIS GameMode, forever — see class
     * comment. Deliberately does NOT go through DeclareWinner/
     * DeclareNoContest/TravelToPhase/TravelToGame: none of the existing
     * travel helpers reopen the CURRENT map with the right GameMode without
     * also mutating session state (TravelToGame) or having no route for a
     * specific game map (TravelToPhase). The ?game= option is mandatory —
     * without it, reopening L_GameC falls back to GlobalDefaultGameMode
     * SILENTLY (see PartyFlowRouter.h's pitfall comment).
     */
    void ReloadCourse();

    /** Read Tuning from the subsystem and apply the bits this GameMode owns (gravity, tutorial length). */
    void LoadAndApplyCourseTuning();

    // ---- Dev tuning menu ------------------------------------------------------

    /** Store the edited tuning and push everything live-appliable into the pawn and camera. */
    void PushLiveTuning();

    /** Move the selection by Delta, skipping header rows. No-op when the menu is closed. */
    void MoveDevMenuSelection(int32 Delta);

    /** Change the selected row's value by Direction (-1/+1) and push it live. */
    void NudgeDevMenuSelection(int32 Direction);

    /** Show/hide the cursor and swap input mode. Keeps the keyboard on gameplay either way. */
    void SetDevMenuInputMode(bool bMenuOpen);

    /** Shared exit path for Tab, Accept and Cancel: restore input and write the ini. */
    void CloseDevMenu();

    /**
     * Row indices are menu-space (headers included) while OctoTuning::GetParams
     * is param-space — this maps one to the other, returning INDEX_NONE for
     * headers and action rows.
     */
    int32 DevMenuRowToParamIndex(int32 RowIndex) const;

    /** Menu-space index of the Accept row. Cancel is the one after it. */
    int32 GetDevMenuAcceptRowIndex() const;

    UPROPERTY()
    TObjectPtr<AOctoPawn> Octo;

    UPROPERTY()
    TObjectPtr<AOctoCamera> Camera;

    /** Guards against multiple overlaps in one frame stacking up travel calls. */
    bool bReloading = false;

    bool  bDevMenuOpen    = false;
    int32 DevMenuSelection = 0;
};
