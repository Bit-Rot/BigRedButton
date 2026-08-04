#pragma once

#include "CoreMinimal.h"
#include "PartyMinigameGameMode.h"
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
 * (APartyMinigameGameMode::OnPlayerButton) entirely: every one of the first
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

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override;
    // Deliberately no TickAI override — see class comment.

    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    TSubclassOf<AOctoPawn> OctoPawnClass;

    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    TSubclassOf<AOctoCamera> OctoCameraClass;

    /** World X the octopus is pinned to — must match AOctoPawn's DOF-locked play plane. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    float PlayPlaneX = 0.f;

    /** Used only if no AOctoSpawnPoint is placed in the level. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FVector FallbackSpawnLocation = FVector(0.f, 300.f, 200.f);

    /** Used only if the roster lookup fails (e.g. L_GameC opened directly with no session). */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FName FallbackMapName = TEXT("L_GameC");

    /**
     * If true, OnPlayerButton/OnPlayerButtonReleased ignore unregistered
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

    UPROPERTY()
    TObjectPtr<AOctoPawn> Octo;

    UPROPERTY()
    TObjectPtr<AOctoCamera> Camera;

    /** Guards against multiple overlaps in one frame stacking up travel calls. */
    bool bReloading = false;
};
