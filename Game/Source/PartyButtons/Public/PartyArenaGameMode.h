#pragma once

#include "CoreMinimal.h"
#include "PartyMinigameGameMode.h"
#include "Math/RandomStream.h"
#include "PartyArenaGameMode.generated.h"

class APartyArena;
class APartyDuelPawn;

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
 *   4. AI-controlled pawns are driven by TickAI, a simple randomized
 *      press/release timer that calls THIS CLASS's own OnPlayerButton/
 *      OnPlayerButtonReleased — the exact same entry point real button input
 *      uses. AI never touches a pawn directly, so it can't do anything a real
 *      player couldn't (see TickAI's comment for why this matters).
 */
UCLASS()
class PARTYBUTTONS_API APartyArenaGameMode : public APartyMinigameGameMode
{
    GENERATED_BODY()

public:
    APartyArenaGameMode();

    virtual FString GetHudSubtitle() const override;

protected:
    virtual void BeginPlay() override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnPlayerButtonReleased(int32 PlayerIndex) override;

    /**
     * Drives every AI-controlled duel pawn through a simple randomized
     * press/release timer (see FDuelAIState / AIState). Intentionally naive —
     * no aiming or positional awareness, just timed charge-and-release — this
     * is a first pass meant for solo testing, not a "smart" opponent.
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

    // ---- AI tunables (dev-only testing aid) --------------------------------
    // The pawn's own charge tunables (MinChargeSeconds etc.) are private, so
    // AI gets its own hold range here. Kept above the pawn's 0.5s MinChargeSeconds
    // so the AI mostly fires real shots instead of cancelling.

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|AI")
    float AIThinkMinSeconds = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|AI")
    float AIThinkMaxSeconds = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|AI")
    float AIHoldMinSeconds = 0.6f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena|AI")
    float AIHoldMaxSeconds = 1.8f;

private:
    void SpawnArena();
    void SpawnPawns();
    void HandlePawnDied(int32 PlayerIndex);

    /** Deterministic-enough distinct color per player index, for the pawn tint. */
    static FLinearColor PlayerColor(int32 PlayerIndex);

    UPROPERTY()
    TObjectPtr<APartyArena> Arena;

    UPROPERTY()
    TMap<int32, TObjectPtr<APartyDuelPawn>> Pawns;

    TSet<int32> AlivePlayers;

    /** Subset of Pawns' keys that are AI-controlled (vs. human), fixed for the round at spawn time. */
    TSet<int32> AIParticipants;

    /** Per-AI-pawn randomized press/release timer state — see TickAI. */
    struct FDuelAIState
    {
        double NextEventTime = 0.0;
        bool   bHolding      = false;
    };
    TMap<int32, FDuelAIState> AIState;

    /** Random stream reused by TickAI for AI timing decisions. */
    FRandomStream AIRng;
};
