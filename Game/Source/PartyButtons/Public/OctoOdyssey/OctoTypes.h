#pragma once

#include "CoreMinimal.h"
#include "OctoTypes.generated.h"

/**
 * Which of OctoOdyssey's two courses a placed actor belongs to.
 *
 * The whole game — menu island, normal course, hard course — lives in ONE level
 * (L_OctoOdyssey), so "which course is this?" can no longer be answered by "which
 * map am I in?" the way it was when each minigame owned a map. Every actor the
 * GameMode discovers by TActorIterator (spawn points, goal flags) therefore
 * carries this tag, and AOctoGameMode matches it against the course the players
 * actually chose. Without it a stray overlap on the far island would end a run on
 * the near one.
 */
UENUM(BlueprintType)
enum class EOctoCourse : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    Hard   UMETA(DisplayName = "Hard"),
};

/** Iteration helper — every course, in menu order. Keep in sync with the enum. */
namespace OctoCourse
{
    inline constexpr EOctoCourse All[] = { EOctoCourse::Normal, EOctoCourse::Hard };
    inline constexpr int32       Num   = UE_ARRAY_COUNT(All);

    inline const TCHAR* ToString(EOctoCourse Course)
    {
        return Course == EOctoCourse::Hard ? TEXT("Hard") : TEXT("Normal");
    }
}

/**
 * What a placed AOctoViewPoint is for.
 *
 * An enum rather than a bool so a second fixed camera (a score-view angle, an
 * attract-mode shot) can be added by dropping another actor in the editor and
 * picking a role, with no new actor class and no code change.
 */
UENUM(BlueprintType)
enum class EOctoViewRole : uint8
{
    /** The fixed camera framing the menu island, with the normal course behind it. */
    MainMenu UMETA(DisplayName = "Main Menu"),
};

/**
 * AOctoGameMode's top-level state. Not a UENUM — nothing reflects it, and unlike
 * EPartyPhase it never crosses a map boundary, because OctoOdyssey never travels:
 * every one of these states is the SAME level viewed from a different camera.
 */
enum class EOctoFlowState : uint8
{
    /** On the menu island. Player buttons cycle the options, main tap selects. */
    MainMenu,

    /** A run is live: the octopus exists, the clock is running, buttons drive arms. */
    Playing,

    /** Finished a run — the table is shown with the player's row editable in place. */
    ScoreEntry,

    /** Both tables shown side by side, read-only (the TOP SCORES menu option). */
    ScoreView,
};
