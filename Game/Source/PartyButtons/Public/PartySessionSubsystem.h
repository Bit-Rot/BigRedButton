#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartySessionState.h"
#include "PartySessionSubsystem.generated.h"

/**
 * UPartySessionSubsystem
 *
 * Thin GameInstanceSubsystem wrapper around FPartySessionState.
 *
 * The GameInstance and its subsystems are the ONLY objects that survive
 * non-seamless map travel (UGameplayStatics::OpenLevel). Everything else
 * (UWorld, AGameModeBase, APlayerController, AHUD, level actors) is torn
 * down and recreated. This subsystem is therefore the correct persistence
 * vehicle for cross-map session data.
 *
 * Usage:
 *   UPartySessionSubsystem* S = UPartySessionSubsystem::Get(this);
 *   S->RegisterPlayer(3);
 *   if (S->IsSessionComplete()) { ... }
 *
 * All GameModes and the HUD go through this subsystem rather than holding
 * a direct pointer to FPartySessionState. The struct itself is not exposed
 * publicly to avoid direct mutation from arbitrary callsites.
 */
UCLASS()
class PARTYBUTTONS_API UPartySessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Convenience getter — avoids boilerplate at every callsite. */
    static UPartySessionSubsystem* Get(const UObject* WorldContext);

    // -----------------------------------------------------------------------
    // Phase
    // -----------------------------------------------------------------------

    EPartyPhase GetPhase() const          { return State.CurrentPhase; }
    void        SetPhase(EPartyPhase P)   { State.CurrentPhase = P; }

    // -----------------------------------------------------------------------
    // Player registration (Lobby)
    // -----------------------------------------------------------------------

    void  RegisterPlayer(int32 PlayerIndex) { State.RegisterPlayer(PlayerIndex); }
    bool  IsPlayerRegistered(int32 PlayerIndex) const { return State.IsPlayerRegistered(PlayerIndex); }
    int32 GetRegisteredCount() const { return State.GetRegisteredCount(); }

    // -----------------------------------------------------------------------
    // Game tracking (Minigame)
    // -----------------------------------------------------------------------

    void  RecordWin(int32 PlayerIndex) { State.RecordWin(PlayerIndex); }
    void  AdvanceGame()                { State.AdvanceGame(); }
    bool  IsSessionComplete() const    { return State.IsSessionComplete(); }

    // -----------------------------------------------------------------------
    // Roster / current game
    // -----------------------------------------------------------------------

    int32 GetCurrentGameIndex() const                   { return State.CurrentGameIndex; }
    void  SetCurrentGame(int32 RosterIndex)             { State.SetCurrentGame(RosterIndex); }
    int32 SelectNextGame(FRandomStream& Rng)            { return State.SelectNextGame(Rng); }
    int32 FindGameByMapName(FName MapName) const        { return State.FindGameByMapName(MapName); }

    const TArray<FPartyGameInfo>& GetRoster() const     { return State.GameRoster; }

    // -----------------------------------------------------------------------
    // Settings
    // -----------------------------------------------------------------------

    int32 GetGamesPerSession() const                    { return State.GamesPerSession; }
    void  SetGamesPerSession(int32 N)                   { State.GamesPerSession = FMath::Max(1, N); }

    // -----------------------------------------------------------------------
    // Results
    // -----------------------------------------------------------------------

    int32 GetLeader() const                             { return State.GetLeader(); }
    int32 GetWinCount(int32 PlayerIndex) const
    {
        return (PlayerIndex >= 0 && PlayerIndex < State.WinCounts.Num())
            ? State.WinCounts[PlayerIndex]
            : 0;
    }
    int32 GetGamesPlayed() const                        { return State.GamesPlayed; }

    // -----------------------------------------------------------------------
    // Session lifecycle
    // -----------------------------------------------------------------------

    /**
     * Clear all live session state (registrations, tallies, games-played).
     * Rules (GamesPerSession) and the roster are preserved.
     * Called at the start of each new game session (MainGameMode or Results).
     */
    void ResetSession() { State.ResetSession(); }

    /**
     * Read-only access to the full state struct for the HUD and tests.
     * Prefer the named accessors above for mutation.
     */
    const FPartySessionState& GetState() const { return State; }

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    FPartySessionState State;
};
