#pragma once

#include "CoreMinimal.h"
#include "PartyMinigameGameMode.h"
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
 *   2. One APartyDuelPawn per Lobby-registered player, or DevFallbackPlayers
 *      players (indices 0..N-1) if none are registered — lets L_GameA be
 *      opened and tested directly without going through the Lobby.
 *   3. Player button events are routed to the matching pawn by PlayerIndex
 *      (pawns are never possessed — see APartyDuelPawn's class comment).
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

    /** Spawned when no players are registered (e.g. opening L_GameA directly in PIE). */
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
};
