#pragma once

#include "CoreMinimal.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoScores.generated.h"

/**
 * One row of a top-ten table: an eight-character name and a run time in seconds.
 *
 * Name is ALWAYS exactly OctoScores::NameLength characters and always drawn from
 * OctoScores::GetAlphabet() — trailing blanks included, never trimmed. That
 * invariant is what lets the HUD draw eight fixed letter boxes and lets player i
 * own letter i unconditionally, with no bounds dance. OctoScores::SanitizeName
 * is the one way to satisfy it; everything that ingests an outside string
 * (the ini, a test) goes through it.
 */
USTRUCT()
struct PARTYBUTTONS_API FOctoScoreEntry
{
    GENERATED_BODY()

    UPROPERTY() FString Name;

    UPROPERTY() float TimeSeconds = 0.f;

    FOctoScoreEntry() = default;
    FOctoScoreEntry(const FString& InName, float InTimeSeconds)
        : Name(InName), TimeSeconds(InTimeSeconds) {}
};

/**
 * OctoScores — the pure, world-free half of OctoOdyssey's scoring.
 *
 * Every function here is a plain function over a TArray: no actor, no world, no
 * subsystem. That is deliberate and it is the same split FOctoTuning/OctoTuning
 * uses — the ranking rules and the file format are exactly the things worth
 * pinning down in unit tests (see PartyButtons.Octo.Scores.*), and they are also
 * exactly the things that would otherwise need a running game to exercise.
 *
 * UOctoScoreSubsystem owns the live tables and the disk path; this namespace owns
 * what a table MEANS.
 *
 * Ordering: ASCENDING by time — fastest first, because this is a race.
 */
namespace OctoScores
{
    /** Rows in a table. Slot 11 (the "you missed it" row) is a HUD-only fiction, never stored. */
    inline constexpr int32 NumSlots = 10;

    /** Letters in a name — and, not coincidentally, one per player button 1..8. */
    inline constexpr int32 NameLength = 8;

    /**
     * The 37 glyphs a name letter can hold: blank, A-Z, 0-9.
     *
     * Blank is FIRST so a fresh name (all blanks) sits at index 0 and a single tap
     * of a button yields 'A' rather than dropping the player into the middle of
     * the digits.
     */
    PARTYBUTTONS_API const TCHAR* GetAlphabet();

    /** 37. Derived from GetAlphabet so the two can never disagree. */
    PARTYBUTTONS_API int32 GetAlphabetLength();

    /** True if Letter is one of the glyphs GetAlphabet offers. */
    PARTYBUTTONS_API bool IsValidLetter(TCHAR Letter);

    /**
     * The next glyph after Current, wrapping in both directions. Direction is
     * usually +1 (a tap or a hold-repeat) but any integer works. A character that
     * is not in the alphabet is treated as blank, so a corrupt name self-heals on
     * first edit rather than sticking.
     */
    PARTYBUTTONS_API TCHAR CycleLetter(TCHAR Current, int32 Direction);

    /** NameLength blanks — the starting state of a name-entry screen. */
    PARTYBUTTONS_API FString BlankName();

    /**
     * Force any string into the name invariant: upper-cased, non-alphabet
     * characters replaced with blanks, padded with blanks or truncated to
     * exactly NameLength.
     */
    PARTYBUTTONS_API FString SanitizeName(const FString& In);

    /**
     * Where TimeSeconds would land in an already-sorted Table, or INDEX_NONE if it
     * is too slow to make the top ten.
     *
     * Ties go AFTER the incumbent: matching a standing time does not displace the
     * name that got there first. That also means a table can never be churned by
     * repeatedly re-posting the same time.
     */
    PARTYBUTTONS_API int32 FindInsertRank(const TArray<FOctoScoreEntry>& Table, float TimeSeconds);

    /**
     * Insert Entry at its rank and trim the table back to NumSlots. A no-op when
     * the entry does not make the cut, so callers can submit unconditionally.
     * Returns the rank it landed at, or INDEX_NONE.
     */
    PARTYBUTTONS_API int32 Insert(TArray<FOctoScoreEntry>& Table, const FOctoScoreEntry& Entry);

    /** Ascending by time. Applied after a load, because the ini is hand-editable. */
    PARTYBUTTONS_API void SortTable(TArray<FOctoScoreEntry>& Table);

    /** "M:SS.mm" — the form the tables and the running clock both display. */
    PARTYBUTTONS_API FString FormatTime(float Seconds);

    /** The single seeded fake score a course starts with, so no table is ever empty. */
    PARTYBUTTONS_API TArray<FOctoScoreEntry> DefaultTable(EOctoCourse Course);

    // ---- Persistence -------------------------------------------------------
    //
    // Driven by a LOCAL FConfigFile, never GConfig. OctoTuning.h:446-461 documents
    // the two bugs that choice exists to dodge (a silently-dropped write into an
    // unloaded filename, and a process-lifetime cache that makes a file written in
    // one PIE session invisible to the next). Both apply verbatim here; this is
    // the same file-shaped problem.
    //
    // Layout — one section per course, so a course can be reset by deleting its
    // section and the other is untouched:
    //
    //     [OctoScores.Normal]
    //     Entry0=OCTOPUS_,90.000000
    //     Entry1=AB__CD__,102.250000
    //
    // Blanks serialise as '_' because FConfigFile trims whitespace off the ends of
    // a value, which would silently shorten any name that starts or ends blank.
    // '_' is not in the alphabet, so the mapping is unambiguous in both directions.

    /** Section name for a course, e.g. "OctoScores.Normal". */
    PARTYBUTTONS_API FString SectionForCourse(EOctoCourse Course);

    /**
     * Write both tables to IniPath, preserving unrelated sections. Returns false
     * if the file could not be written — callers MUST surface that rather than
     * assume success (FConfigFile::Write returns true when it skips a non-dirty
     * write, which is the trap).
     */
    PARTYBUTTONS_API bool SaveToIni(
        const TArray<FOctoScoreEntry>& NormalTable,
        const TArray<FOctoScoreEntry>& HardTable,
        const FString&                 IniPath);

    /**
     * Overlay IniPath onto the two tables, which should already hold their
     * defaults. A course whose section is absent is LEFT ALONE — so a new course
     * needs no migration, and deleting a section is a per-course reset. Loaded
     * tables are sanitized, sorted and trimmed to NumSlots, because the file is
     * meant to be hand-editable.
     *
     * Returns the number of entries read.
     */
    PARTYBUTTONS_API int32 LoadFromIni(
        const FString&           IniPath,
        TArray<FOctoScoreEntry>& InOutNormalTable,
        TArray<FOctoScoreEntry>& InOutHardTable);
}
