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

/**
 * Collision setup shared by everything in OctoOdyssey that is a trigger.
 */
namespace OctoCollision
{
    /**
     * Collision profile for every trigger volume in the game -- AOctoTriggerVolume's
     * shapes and AOctoGoalFlag's. Defined in Game/Config/DefaultEngine.ini alongside
     * the OctoTrigger OBJECT channel it uses.
     *
     * NOT the stock "Trigger" profile, and the difference is not cosmetic. Stock
     * Trigger is QueryOnly with every response set to Overlap, which makes it
     * correctly non-solid to the physics scene -- but its OBJECT TYPE is
     * WorldDynamic, and AOctoPawn sweeps its arms and head with
     * SweepSingleByObjectType(WorldStatic | WorldDynamic). An object query filters
     * on object type alone and reports every match as a BLOCKING hit; the profile's
     * responses are never read. So a stock-Trigger checkpoint let the body sphere
     * float through it exactly as intended while an extending arm planted on its
     * invisible faces, took EAchieved = E + Hit.Distance, and fired a push-off
     * impulse at the phantom impact point -- an off-centre shove out of thin air,
     * from a different face depending on where the octopus happened to be.
     *
     * OctoTrigger fixes that at the source: same QueryOnly, same overlap-everything
     * responses, but an object type those sweeps do not ask for. The volumes vanish
     * from the sweeps and overlap detection is untouched -- AOctoPawn::BodySphere is
     * a PhysicsActor (object type PhysicsBody), the profile overlaps PhysicsBody,
     * and the channel's own default response is Overlap.
     *
     * A new sweep that needs to see real geometry must therefore never simply add
     * "every object type" -- see AOctoPawn::TickArm.
     */
    inline const TCHAR* const TriggerProfile = TEXT("OctoTrigger");
}
