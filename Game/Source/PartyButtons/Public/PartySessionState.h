#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "PartyTypes.h"
#include "PartySessionState.generated.h"

/**
 * FPartySessionState
 *
 * Pure, world-free data model for a PartyButtons play session.
 * Holds which players joined, their win tallies, how many games have been played,
 * the configurable rules, and the 16-entry game roster.
 *
 * All logic lives here (not in the subsystem), making it fully unit-testable
 * without a world, a GameInstance, or any UObject overhead — just stack-allocate it.
 *
 * Owned by UPartySessionSubsystem, which is a GameInstanceSubsystem that survives
 * non-seamless map travel (OpenLevel). Every other system (GameModes, HUD) reads
 * and mutates state through the subsystem's pass-through accessors.
 *
 * Player model reminder:
 *   PlayerIndex is a 0-based dispatch index (0 = button 1 = player 1).
 *   It is NOT a ULocalPlayer. There is ONE PlayerController that fans out by index.
 */
USTRUCT()
struct PARTYBUTTONS_API FPartySessionState
{
    GENERATED_BODY()

    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    static constexpr int32 NUM_PLAYERS = 16;

    // -----------------------------------------------------------------------
    // Configurable rules (settable from APartySettingsGameMode)
    // -----------------------------------------------------------------------

    /** Session ends after this many games have been played. */
    UPROPERTY()
    int32 GamesPerSession = 10;

    // -----------------------------------------------------------------------
    // Game roster (filled once by InitDefaultRoster; constant during a session)
    // -----------------------------------------------------------------------

    /** 16-entry roster: Id 0..15, zany DisplayName, MapName L_GameA..L_GameP. */
    UPROPERTY()
    TArray<FPartyGameInfo> GameRoster;

    // -----------------------------------------------------------------------
    // Live session state (mutated by gameplay methods below)
    // -----------------------------------------------------------------------

    /** Bit per player: true if this player pressed their button in the lobby window. */
    UPROPERTY()
    TArray<bool> RegisteredPlayers;

    /** Win count per player across the current session. */
    UPROPERTY()
    TArray<int32> WinCounts;

    /** Number of minigames completed so far this session. */
    UPROPERTY()
    int32 GamesPlayed = 0;

    /** Roster index of the currently active minigame (set by LevelSelect). INDEX_NONE when none. */
    UPROPERTY()
    int32 CurrentGameIndex = INDEX_NONE;

    /** Current high-level phase, kept in sync by APartyGameModeBase::TravelToPhase. */
    UPROPERTY()
    EPartyPhase CurrentPhase = EPartyPhase::Main;

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    FPartySessionState();

    // -----------------------------------------------------------------------
    // Initialization / reset
    // -----------------------------------------------------------------------

    /** Fill GameRoster with the 16 default games (A–P). Called from the ctor. */
    void InitDefaultRoster();

    /**
     * Clear live session state: zero RegisteredPlayers, WinCounts, GamesPlayed,
     * CurrentGameIndex. Resets phase to MainMenu. Does NOT change GamesPerSession
     * or GameRoster (rules + roster persist across resets).
     */
    void ResetSession();

    // -----------------------------------------------------------------------
    // Player registration (Lobby phase)
    // -----------------------------------------------------------------------

    /**
     * Mark PlayerIndex as joined for this session.
     * Idempotent. Out-of-range indices are silently ignored.
     */
    void RegisterPlayer(int32 PlayerIndex);

    bool IsPlayerRegistered(int32 PlayerIndex) const;

    /** Number of players who have registered so far (0..NUM_PLAYERS). */
    int32 GetRegisteredCount() const;

    // -----------------------------------------------------------------------
    // Game tracking (Minigame phase)
    // -----------------------------------------------------------------------

    /**
     * Increment WinCounts[PlayerIndex].
     * Out-of-range is silently ignored.
     */
    void RecordWin(int32 PlayerIndex);

    /** Increment GamesPlayed. */
    void AdvanceGame();

    /** True when GamesPlayed >= GamesPerSession. */
    bool IsSessionComplete() const;

    // -----------------------------------------------------------------------
    // Roster access
    // -----------------------------------------------------------------------

    /** Set CurrentGameIndex to the chosen roster entry. */
    void SetCurrentGame(int32 RosterIndex);

    /**
     * Return a pseudo-random roster index from Rng, preferring to avoid the
     * just-played game if possible (to prevent immediate repeats).
     */
    int32 SelectNextGame(FRandomStream& Rng) const;

    /**
     * Find the roster index whose MapName matches MapName.
     * Returns INDEX_NONE if not found.
     * Comparison is on the short package name (e.g. "L_GameA"), not a full path.
     */
    int32 FindGameByMapName(FName MapName) const;

    // -----------------------------------------------------------------------
    // Results
    // -----------------------------------------------------------------------

    /**
     * Return the PlayerIndex with the highest WinCount.
     * Ties are broken by lowest index (deterministic).
     * Returns INDEX_NONE if no games have been won yet.
     */
    int32 GetLeader() const;

private:
    bool IsValidPlayerIndex(int32 PlayerIndex) const
    {
        return PlayerIndex >= 0 && PlayerIndex < NUM_PLAYERS;
    }
};
