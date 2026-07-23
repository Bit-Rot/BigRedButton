#pragma once

#include "CoreMinimal.h"
#include "PartyTypes.generated.h"

/**
 * High-level game phases. Each phase corresponds to a map and a GameMode.
 * The current phase is stored in FPartySessionState and survives map travel
 * via UPartySessionSubsystem (a GameInstanceSubsystem).
 */
UENUM(BlueprintType)
enum class EPartyPhase : uint8
{
    Main        UMETA(DisplayName = "Main"),         // startup: init + immediate redirect
    MainMenu    UMETA(DisplayName = "Main Menu"),    // Play / Settings
    Settings    UMETA(DisplayName = "Settings"),     // configurable options stub
    Lobby       UMETA(DisplayName = "Lobby"),        // players join via countdown
    LevelSelect UMETA(DisplayName = "Level Select"), // Roulette Rush minigame
    Minigame    UMETA(DisplayName = "Minigame"),     // one of the 16 game maps
    Results     UMETA(DisplayName = "Results"),      // end-of-session leaderboard
};

/**
 * Entry in the game roster. 16 entries (A–P), one per minigame map.
 * Stored in FPartySessionState.GameRoster, initialized by InitDefaultRoster().
 */
USTRUCT(BlueprintType)
struct PARTYBUTTONS_API FPartyGameInfo
{
    GENERATED_BODY()

    /** 0-based roster index (0 = GameA, …, 15 = GameP). */
    UPROPERTY(BlueprintReadOnly)
    int32 Id = 0;

    /** Zany alliterative display name shown on the minigame map and in LevelSelect. */
    UPROPERTY(BlueprintReadOnly)
    FString DisplayName;

    /** Short map package name, e.g. "L_GameA". Used for OpenLevel travel. */
    UPROPERTY(BlueprintReadOnly)
    FName MapName;

    /**
     * Optional per-game GameMode override, reflected class path (no 'A' prefix), e.g.
     * "/Script/PartyButtons.PartyArenaGameMode". Empty means "use the shared
     * PartyMinigameGameMode" (EPartyPhase::Minigame's route). Read by
     * APartyGameModeBase::TravelToGame.
     */
    UPROPERTY(BlueprintReadOnly)
    FString GameModeClassPath;
};

/**
 * Maps a phase (or a specific game map) to its travel destination.
 * Returned by PartyFlow::GetRoute(); used by APartyGameModeBase::TravelToPhase.
 *
 * GameModeClassPath is the script path WITHOUT the 'A' prefix, e.g.:
 *   "/Script/PartyButtons.PartyMinigameGameMode"
 * This matches the reflected class name that OpenLevel's ?game= option accepts.
 */
struct FPartyPhaseRoute
{
    /** The destination map's short name (e.g. "L_Lobby"). NAME_None for Minigame
     *  because the map is determined per-selected-game. */
    FName MapName;

    /** Full script class path for the ?game= travel option. Never empty. */
    FString GameModeClassPath;
};
