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
 * Sub-phase for a minigame that opts into a tutorial/discovery intro before
 * gameplay proper. Consumed by APartyFlowHUD::DrawMinigame to switch drawing
 * modes. Games that don't opt in return EPartyOverlayPhase::None from
 * APartyGameModeBase::GetOverlayPhase (the default) and get the legacy
 * full-screen title + subtitle overlay.
 */
UENUM(BlueprintType)
enum class EPartyOverlayPhase : uint8
{
    None      UMETA(DisplayName = "None"),      // legacy: full-screen title/subtitle
    Tutorial  UMETA(DisplayName = "Tutorial"),  // centered dialog, gameplay frozen
    Discovery UMETA(DisplayName = "Discovery"), // numbers above pawns, countdown beeps
    Playing   UMETA(DisplayName = "Playing"),   // no overlay — arena is live
};

/**
 * One number-above-pawn marker projected by APartyFlowHUD during the discovery
 * phase (see APartyArenaGameMode). WorldLocation is projected to screen space;
 * bHeld toggles the number's color to indicate "yes, that's your button."
 */
USTRUCT()
struct PARTYBUTTONS_API FPartyPawnMarker
{
    GENERATED_BODY()

    UPROPERTY() FVector       WorldLocation = FVector::ZeroVector;
    UPROPERTY() int32         PlayerIndex   = INDEX_NONE;
    UPROPERTY() FLinearColor  Color         = FLinearColor::White;
    UPROPERTY() bool          bHeld         = false;
};

/**
 * One row of the dev tuning menu (Tab), rendered by APartyFlowHUD::DrawDevMenu.
 *
 * The menu is deliberately generic: the HUD knows how to draw a list of labelled
 * rows with optional sliders, and nothing else. Which rows exist, what they mean
 * and what changing one does is entirely up to the GameMode supplying them via
 * APartyGameModeBase::GetDevMenuRows — which is what keeps the HUD from ever
 * having to cast to a concrete GameMode, and what lets any minigame get the same
 * menu for the cost of overriding a few virtuals.
 */
USTRUCT()
struct PARTYBUTTONS_API FPartyDevMenuRow
{
    GENERATED_BODY()

    /** Left-hand text. For a header this is the section name; for an action, the button caption. */
    UPROPERTY() FString Label;

    /** Right-hand text ("900.0", "On"). Empty for headers and actions. */
    UPROPERTY() FString ValueText;

    /** Slider fill, 0..1. Negative means "draw no slider" — used by headers and actions. */
    UPROPERTY() float Normalized = -1.f;

    /** A section heading: not selectable, drawn in the accent colour, no slider. */
    UPROPERTY() bool bIsHeader = false;

    /** A button (Accept / Cancel): selectable and clickable, but has no value to drag. */
    UPROPERTY() bool bIsAction = false;

    /** Appended to the label in a dimmer colour, e.g. "(restart)". */
    UPROPERTY() FString Note;
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
