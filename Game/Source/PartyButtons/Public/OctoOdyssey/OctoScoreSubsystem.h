#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OctoOdyssey/OctoScores.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoScoreSubsystem.generated.h"

/**
 * UOctoScoreSubsystem
 *
 * Holds OctoOdyssey's two live top-ten tables (normal and hard) and owns their
 * one on-disk copy, Config/OctoScores.ini.
 *
 * A GameInstanceSubsystem for the same reason UOctoTuningSubsystem is one: the
 * GameInstance outlives the world. OctoOdyssey no longer travels between maps,
 * so that is less load-bearing than it was — but PIE tears the world down and
 * rebuilds it on every run, and a table that lived on the GameMode would reset
 * every time the developer pressed Play, which is exactly the bug that makes
 * scoring look like it "doesn't save".
 *
 * Reads on Initialize, writes on every SubmitScore. Writing eagerly rather than
 * on shutdown is deliberate: an arcade cabinet gets switched off at the wall, and
 * a save that only happens on a clean exit is a save that never happens.
 *
 * The ranking rules and the file format live in OctoScores (pure functions,
 * unit-tested); this class only decides WHEN they run and WHERE the file is.
 */
UCLASS()
class PARTYBUTTONS_API UOctoScoreSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Convenience getter. Returns nullptr outside a game world (an editor
     * viewport has no GameInstance); callers fall back to OctoScores::DefaultTable
     * rather than treating that as an error.
     */
    static UOctoScoreSubsystem* Get(const UObject* WorldContext);

    /** The live table for a course, fastest first. Never empty — see OctoScores::DefaultTable. */
    const TArray<FOctoScoreEntry>& GetTable(EOctoCourse Course) const;

    /**
     * Would this time make the board? Returns the rank it would take, or
     * INDEX_NONE. The HUD asks this to decide between "CONGRATULATIONS" with an
     * editable row and "BETTER LUCK NEXT TIME" with the unnumbered slot-11 row.
     */
    int32 FindInsertRank(EOctoCourse Course, float TimeSeconds) const;

    /**
     * Insert the entry if it qualifies and write the ini. Returns the rank it
     * landed at, or INDEX_NONE if it missed (in which case nothing is stored and
     * nothing is written).
     */
    int32 SubmitScore(EOctoCourse Course, const FOctoScoreEntry& Entry);

    /** Back to one seeded fake score per course, and delete the ini. */
    void ResetTables();

    /** Write both tables to Config/OctoScores.ini. */
    void SaveToIni() const;

    /** Seed the defaults, then overlay Config/OctoScores.ini onto them. */
    void LoadFromIni();

    /** Absolute path of the score ini. Public so tests and logs can name it. */
    static FString GetIniPath();

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    /** Non-const accessor for the mutating paths. Same lookup as GetTable. */
    TArray<FOctoScoreEntry>& TableFor(EOctoCourse Course);

    TArray<FOctoScoreEntry> NormalTable;
    TArray<FOctoScoreEntry> HardTable;
};
