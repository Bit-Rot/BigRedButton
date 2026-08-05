#pragma once

#include "CoreMinimal.h"
#include "PartyMinigameGameMode.h"
#include "PartyDuelAI.h"
#include "Math/RandomStream.h"
#include "PartyArenaGameMode.generated.h"

class APartyArena;
class APartyDuelPawn;
class APartyBullet;
class USoundBase;

/** Selects an FDuelAIParams behavior profile (PartyDuelAI::EasyParams/MediumParams/HardParams). */
UENUM()
enum class EPartyAIDifficulty : uint8
{
    Easy,
    Medium,
    Hard,
};

/**
 * APartyArenaGameMode
 *
 * GameMode for L_GameA ("Reflex Rumble", roster slot 0) — a spin, charge, and
 * reflect duel. Overrides the shared "first press wins" rule
 * (APartyMinigameGameMode::OnPlayerButton) with "last player standing wins",
 * declared via the inherited DeclareWinner()/DeclareNoContest() so the
 * record-win / advance-session / travel plumbing stays shared with the other
 * 15 minigames.
 *
 * Setup (BeginPlay, after Super::BeginPlay() resolves the roster entry for the HUD):
 *   1. Spawn one APartyArena and view through its top-down camera.
 *   2. One APartyDuelPawn per Lobby-registered player, PLUS one per AI-filled
 *      slot (FPartySessionState::NumAIPlayers, dev-only Lobby Up/Down control —
 *      see FPartySessionState::ComputeAISlots). Falls back to DevFallbackPlayers
 *      (indices 0..N-1, human-controlled) only when NEITHER humans nor AI are
 *      configured — lets L_GameA be opened and tested directly without going
 *      through the Lobby.
 *   3. Player button events are routed to the matching pawn by PlayerIndex
 *      (pawns are never possessed — see APartyDuelPawn's class comment).
 *   4. AI-controlled pawns are driven by TickAI, a reflex state machine (see
 *      PartyDuelAI.h for the pure decision math) that calls THIS CLASS's own
 *      OnPlayerButton/OnPlayerButtonReleased — the exact same entry point real
 *      button input uses. AI never touches a pawn directly, so it can't do
 *      anything a real player couldn't (see TickAI's comment for why this
 *      matters). Behavior — aim error, reaction latency, fire cadence,
 *      ricochet chance, block chance — is fully tunable via FDuelAIParams and
 *      selected per-round by AIDifficulty (Easy/Medium/Hard); Medium is tuned
 *      to be believable and beatable, not an aimbot.
 */
UCLASS()
class PARTYBUTTONS_API APartyArenaGameMode : public APartyMinigameGameMode
{
    GENERATED_BODY()

public:
    APartyArenaGameMode();

    virtual FString GetHudSubtitle() const override;

    // ---- Intro overlay (tutorial + discovery) --------------------------------
    virtual EPartyOverlayPhase        GetOverlayPhase() const override;
    virtual float                     GetTutorialRemainingSeconds() const override;
    virtual TArray<FPartyPawnMarker>  GetPawnMarkers() const override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override;

    /**
     * Drives every AI-controlled duel pawn through a reflex state machine:
     * defend first (block a threatening bullet if in range and the block-
     * chance roll passes), otherwise plan and take a shot (direct or
     * ricochet, aimed with jitter, timed to the pawn's spin) — see
     * PartyDuelAI.h for the underlying pure math and FDuelAIState below for
     * the per-pawn plan/timer state.
     *
     * CRITICAL: this must call OnPlayerButton(i)/OnPlayerButtonReleased(i) —
     * this class's OWN overridden methods, the exact entry point real button
     * input calls via the delegate chain — never Pawn->NotifyPressed()/
     * NotifyReleased() directly. That's what makes "AI uses the same input
     * pathways a player would, no cheating" true by construction rather than
     * by convention.
     */
    virtual void TickAI(float DeltaSeconds) override;

    /** Spawned only when NEITHER humans nor AI are configured (e.g. opening L_GameA directly in PIE). */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    int32 DevFallbackPlayers = 4;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    TSubclassOf<APartyArena> ArenaClass;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    TSubclassOf<APartyDuelPawn> DuelPawnClass;

    /** Rejection-sampling minimum spacing between spawn points, in meters. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float MinSpawnSeparationMeters = 1.5f;

    /** Player collision-sphere radius, used for spawn placement (see APartyDuelPawn::SphereRadiusMeters). */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float PlayerRadiusMeters = 0.25f;

    /** Behavior profile for every AI pawn this round — see PartyDuelAI::EasyParams/MediumParams/HardParams. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|AI")
    EPartyAIDifficulty AIDifficulty = EPartyAIDifficulty::Medium;

    // ---- Intro overlay tunables ---------------------------------------------
    // Tutorial dismisses at whichever comes first: TutorialMaxSeconds elapsed,
    // OR every human participant is currently holding their button.

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|Intro")
    float TutorialMaxSeconds = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|Intro")
    float DiscoverySeconds = 3.f;

    /** Countdown one-shot for the two lead-in beeps. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|Intro")
    TObjectPtr<USoundBase> BeepSound;

    /** Countdown one-shot for the final "BOOOP" — game starts as this plays. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|Intro")
    TObjectPtr<USoundBase> BoopSound;

private:
    enum class ESubPhase : uint8 { Tutorial, Discovery, Playing };

    void SpawnArena();
    void SpawnPawns();
    void HandlePawnDied(int32 PlayerIndex);

    void EnterDiscovery();
    void EnterPlaying();
    void SetPawnsFrozen(bool bFrozen);
    void PlayCountdownBeat(bool bIsBoop);
    bool AllHumansHolding() const;

    /** Deterministic-enough distinct color per player index, for the pawn tint. */
    static FLinearColor PlayerColor(int32 PlayerIndex);

    /** Nearest living opponent to SelfIndex, or nullptr if none (degenerate round). */
    APartyDuelPawn* FindNearestOpponent(int32 SelfIndex) const;

    /** Resolves AIDifficulty to its FDuelAIParams profile — shared by SpawnPawns (initial stagger) and TickAI. */
    PartyDuelAI::FDuelAIParams GetActiveAIParams() const;

    struct FDuelAIState;

    /** If a bullet threatens SelfPawn within the block horizon and the block-chance roll passes, returns true. */
    bool ShouldBlockNow(const APartyDuelPawn& SelfPawn, const PartyDuelAI::FDuelAIParams& Params) const;

    /** Picks a target, decides direct-vs-ricochet, aims (with jitter), and commits a shot plan into State. Returns false if no shot was committed (no target, or the fire-chance roll failed). */
    bool PlanShot(int32 SelfIndex, const APartyDuelPawn& SelfPawn, const PartyDuelAI::FDuelAIParams& Params, double Now, FDuelAIState& State);

    UPROPERTY()
    TObjectPtr<APartyArena> Arena;

    UPROPERTY()
    TMap<int32, TObjectPtr<APartyDuelPawn>> Pawns;

    TSet<int32> AlivePlayers;

    /** Subset of Pawns' keys that are AI-controlled (vs. human), fixed for the round at spawn time. */
    TSet<int32> AIParticipants;

    /** Per-AI-pawn reflex state: current shot plan/charge timer, and a pending defensive block tap. */
    struct FDuelAIState
    {
        // ---- Offense: the currently committed shot plan, if any ----
        bool   bPlanCommitted    = false;
        float  PlannedBearingDeg = 0.f;
        float  PlannedHoldSeconds = 0.f;
        bool   bHolding          = false;
        double PressTime         = 0.0;

        /** Reaction-latency gate: no new plan/decision before this time. */
        double NextDecisionTime  = 0.0;

        // ---- Defense: a block tap in progress (pressed this tick, release next) ----
        bool   bBlockPending     = false;
    };
    TMap<int32, FDuelAIState> AIState;

    /** Random stream reused by TickAI for all AI decisions (targeting, jitter, wall choice). */
    FRandomStream AIRng;

    /** Bullet collision radius (units/cm), cached once for ricochet-bounce-box math — see SpawnPawns. */
    float BulletRadiusUnitsForPlanning = 8.f;

    // ---- Intro overlay state -------------------------------------------------
    ESubPhase   SubPhase        = ESubPhase::Tutorial;
    float       SubPhaseElapsed = 0.f;
    TSet<int32> HeldButtons;         // any participant currently holding — for tutorial gate + discovery tint
    TSet<int32> HumanParticipants;   // subset of Pawns keys that are NOT AI — used by AllHumansHolding
    int32       NextCountdownBeat = 0;
};
