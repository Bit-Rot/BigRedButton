#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "OctoOdyssey/OctoScores.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Scores.*
//
// The ranking rules and the file format for OctoOdyssey's two top-ten tables.
// No world, no actor: OctoScores is deliberately a namespace of plain functions
// precisely so the rules that decide who gets on the board are checkable without
// running the game (see OctoScores.h).
//
// The rules worth pinning down are the ones a player would notice being wrong:
// ascending order (this is a race), a tie not stealing someone else's slot, and
// a table that never grows past ten.
// --------------------------------------------------------------------------

namespace
{
    /** A table of N entries at 10, 20, 30... seconds — sorted, as every table is. */
    TArray<FOctoScoreEntry> MakeTable(int32 Count)
    {
        TArray<FOctoScoreEntry> Table;
        for (int32 i = 0; i < Count; i++)
        {
            Table.Add(FOctoScoreEntry(OctoScores::SanitizeName(FString::Printf(TEXT("NAME%d"), i)),
                                      10.f * (i + 1)));
        }
        return Table;
    }

    /** A scratch ini path unique to the running test, cleaned up by the caller. */
    FString MakeScratchIniPath(const TCHAR* Tag)
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::AutomationTransientDir(),
                FString::Printf(TEXT("OctoScoresTest_%s_%s.ini"), Tag, *FGuid::NewGuid().ToString())));
    }
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresRankIsAscending,
    "PartyButtons.Octo.Scores.RankIsAscending",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresRankIsAscending::RunTest(const FString& Parameters)
{
    const TArray<FOctoScoreEntry> Full = MakeTable(OctoScores::NumSlots); // 10..100s

    TestEqual(TEXT("Faster than everything takes rank 0"),
        OctoScores::FindInsertRank(Full, 1.f), 0);

    TestEqual(TEXT("Between slots 0 and 1 takes rank 1"),
        OctoScores::FindInsertRank(Full, 15.f), 1);

    TestEqual(TEXT("Faster than only the last takes rank 9"),
        OctoScores::FindInsertRank(Full, 99.f), 9);

    // The whole point of the INDEX_NONE branch: a full board rejects a slow time
    // rather than silently keeping an eleventh row.
    TestEqual(TEXT("Slower than a full board misses entirely"),
        OctoScores::FindInsertRank(Full, 101.f), (int32)INDEX_NONE);

    // A short table has room for anything, however slow.
    const TArray<FOctoScoreEntry> Short = MakeTable(3);
    TestEqual(TEXT("A slow time still places on a table with room"),
        OctoScores::FindInsertRank(Short, 9999.f), 3);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresTieDoesNotDisplaceIncumbent,
    "PartyButtons.Octo.Scores.TieDoesNotDisplaceIncumbent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresTieDoesNotDisplaceIncumbent::RunTest(const FString& Parameters)
{
    TArray<FOctoScoreEntry> Table = MakeTable(3); // 10, 20, 30

    // Matching slot 1's time must rank BELOW it, not push it down. Otherwise
    // re-posting the same time repeatedly would churn a name off the board it
    // never actually beat.
    const int32 Rank = OctoScores::Insert(Table, FOctoScoreEntry(OctoScores::SanitizeName(TEXT("TIED")), 20.f));

    TestEqual(TEXT("A tie lands after the incumbent"), Rank, 2);
    TestEqual(TEXT("The incumbent keeps its slot"), Table[1].Name, OctoScores::SanitizeName(TEXT("NAME1")));
    TestEqual(TEXT("The tie sits directly below it"), Table[2].Name, OctoScores::SanitizeName(TEXT("TIED")));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresInsertTruncatesToTen,
    "PartyButtons.Octo.Scores.InsertTruncatesToTen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresInsertTruncatesToTen::RunTest(const FString& Parameters)
{
    TArray<FOctoScoreEntry> Table = MakeTable(OctoScores::NumSlots);

    const int32 Rank = OctoScores::Insert(Table, FOctoScoreEntry(OctoScores::SanitizeName(TEXT("FASTEST")), 1.f));

    TestEqual(TEXT("The new best takes rank 0"), Rank, 0);
    TestEqual(TEXT("The table is still exactly ten rows"), Table.Num(), OctoScores::NumSlots);
    TestEqual(TEXT("The slowest entry was pushed off"), Table.Last().TimeSeconds, 90.f);

    // A miss must leave the table completely alone — callers submit
    // unconditionally and rely on this.
    const TArray<FOctoScoreEntry> Before = Table;
    const int32 MissRank = OctoScores::Insert(Table, FOctoScoreEntry(OctoScores::SanitizeName(TEXT("SLOW")), 500.f));

    TestEqual(TEXT("A miss reports INDEX_NONE"), MissRank, (int32)INDEX_NONE);
    TestEqual(TEXT("A miss changes nothing"), Table.Num(), Before.Num());
    TestEqual(TEXT("A miss leaves the last row untouched"), Table.Last().Name, Before.Last().Name);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresLetterCycleWraps,
    "PartyButtons.Octo.Scores.LetterCycleWraps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresLetterCycleWraps::RunTest(const FString& Parameters)
{
    const int32 Num = OctoScores::GetAlphabetLength();

    TestEqual(TEXT("The alphabet is blank + A-Z + 0-9"), Num, 37);
    TestTrue(TEXT("Blank is a valid letter"), OctoScores::IsValidLetter(TEXT(' ')));
    TestTrue(TEXT("'A' is a valid letter"),   OctoScores::IsValidLetter(TEXT('A')));
    TestTrue(TEXT("'0' is a valid letter"),   OctoScores::IsValidLetter(TEXT('0')));
    TestFalse(TEXT("Lower case is not"),      OctoScores::IsValidLetter(TEXT('a')));
    TestFalse(TEXT("The disk blank stand-in is not"), OctoScores::IsValidLetter(TEXT('_')));

    // Blank first means one tap off an empty name gives 'A', not a digit.
    TestEqual(TEXT("Blank advances to A"), OctoScores::CycleLetter(TEXT(' '), +1), TEXT('A'));
    TestEqual(TEXT("Z advances to 0"),     OctoScores::CycleLetter(TEXT('Z'), +1), TEXT('0'));

    // Negative directions are the case a bare % would get wrong (C++ keeps the
    // dividend's sign), so it is checked explicitly.
    TestEqual(TEXT("Blank steps back to 9"), OctoScores::CycleLetter(TEXT(' '), -1), TEXT('9'));
    TestEqual(TEXT("A steps back to blank"), OctoScores::CycleLetter(TEXT('A'), -1), TEXT(' '));

    // A full lap returns to where it started, for every glyph.
    for (int32 i = 0; i < Num; i++)
    {
        const TCHAR Start = OctoScores::GetAlphabet()[i];
        TCHAR Current = Start;
        for (int32 Step = 0; Step < Num; Step++)
        {
            Current = OctoScores::CycleLetter(Current, +1);
        }
        TestEqual(TEXT("A full lap returns to the starting glyph"), Current, Start);
    }

    // A character from outside the alphabet self-heals to the start rather than
    // sticking, so a hand-mangled ini cannot produce an uneditable name.
    TestEqual(TEXT("An unknown glyph is treated as blank"), OctoScores::CycleLetter(TEXT('#'), +1), TEXT('A'));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresNameInvariant,
    "PartyButtons.Octo.Scores.NameInvariant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresNameInvariant::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("A blank name is exactly eight characters"),
        OctoScores::BlankName().Len(), OctoScores::NameLength);

    TestEqual(TEXT("A short name is padded"),      OctoScores::SanitizeName(TEXT("AB")).Len(), 8);
    TestEqual(TEXT("An over-long name is cut"),    OctoScores::SanitizeName(TEXT("ABCDEFGHIJ")), FString(TEXT("ABCDEFGH")));
    TestEqual(TEXT("Lower case is raised"),        OctoScores::SanitizeName(TEXT("abc")), FString(TEXT("ABC     ")));
    TestEqual(TEXT("Illegal glyphs become blank"), OctoScores::SanitizeName(TEXT("A#B")), FString(TEXT("A B     ")));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresFormatTime,
    "PartyButtons.Octo.Scores.FormatTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresFormatTime::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Zero"),              OctoScores::FormatTime(0.f),      FString(TEXT("0:00.00")));
    TestEqual(TEXT("Sub-minute"),        OctoScores::FormatTime(9.5f),     FString(TEXT("0:09.50")));
    TestEqual(TEXT("Exactly a minute"),  OctoScores::FormatTime(60.f),     FString(TEXT("1:00.00")));
    TestEqual(TEXT("Minutes and change"),OctoScores::FormatTime(95.25f),   FString(TEXT("1:35.25")));

    // The reason the rounding happens ONCE into hundredths: rounding the fields
    // separately produces "1:60.00" here.
    TestEqual(TEXT("Rounding up carries into the minute"),
        OctoScores::FormatTime(59.999f), FString(TEXT("1:00.00")));

    TestEqual(TEXT("Negative clamps to zero"), OctoScores::FormatTime(-5.f), FString(TEXT("0:00.00")));

    return true;
}

// --------------------------------------------------------------------------
// Persistence. Goes all the way to a real file on disk on purpose — the same
// reasoning as PartyButtons.Octo.Tuning.IniRoundTrips: the failure this guards
// against is a write that reports success and produces nothing, which a
// read-only test against a hand-written ini would never see.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresIniRoundTrips,
    "PartyButtons.Octo.Scores.IniRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresIniRoundTrips::RunTest(const FString& Parameters)
{
    const FString IniPath = MakeScratchIniPath(TEXT("RoundTrip"));
    IFileManager::Get().Delete(*IniPath);

    // ---- Save ------------------------------------------------------------

    TArray<FOctoScoreEntry> SavedNormal;
    SavedNormal.Add(FOctoScoreEntry(OctoScores::SanitizeName(TEXT("ALPHA")),  42.25f));
    SavedNormal.Add(FOctoScoreEntry(OctoScores::SanitizeName(TEXT("BRAVO")),  61.5f));

    // Leading AND trailing blanks: FConfigFile trims whitespace off a value, so a
    // name stored as raw spaces would come back short. That is the entire reason
    // for the '_' stand-in on disk.
    TArray<FOctoScoreEntry> SavedHard;
    SavedHard.Add(FOctoScoreEntry(FString(TEXT(" MID X  ")), 123.75f));

    TestTrue(TEXT("SaveToIni reports success"),
        OctoScores::SaveToIni(SavedNormal, SavedHard, IniPath));

    TestTrue(TEXT("The ini actually exists on disk"), IFileManager::Get().FileExists(*IniPath));

    FString FileContents;
    TestTrue(TEXT("The ini is readable"), FFileHelper::LoadFileToString(FileContents, *IniPath));
    TestTrue(TEXT("The ini has the normal section"),
        FileContents.Contains(FString::Printf(TEXT("[%s]"), *OctoScores::SectionForCourse(EOctoCourse::Normal))));
    TestTrue(TEXT("The ini has the hard section"),
        FileContents.Contains(FString::Printf(TEXT("[%s]"), *OctoScores::SectionForCourse(EOctoCourse::Hard))));

    // ---- Load ------------------------------------------------------------

    // Start from the defaults, exactly as UOctoScoreSubsystem does.
    TArray<FOctoScoreEntry> LoadedNormal = OctoScores::DefaultTable(EOctoCourse::Normal);
    TArray<FOctoScoreEntry> LoadedHard   = OctoScores::DefaultTable(EOctoCourse::Hard);

    const int32 NumLoaded = OctoScores::LoadFromIni(IniPath, LoadedNormal, LoadedHard);

    TestEqual(TEXT("Three entries were read"), NumLoaded, 3);

    TestEqual(TEXT("Normal has two rows"),  LoadedNormal.Num(), 2);
    TestEqual(TEXT("Normal row 0 name"),    LoadedNormal[0].Name, OctoScores::SanitizeName(TEXT("ALPHA")));
    TestEqual(TEXT("Normal row 0 time"),    LoadedNormal[0].TimeSeconds, 42.25f);
    TestEqual(TEXT("Normal row 1 name"),    LoadedNormal[1].Name, OctoScores::SanitizeName(TEXT("BRAVO")));

    TestEqual(TEXT("Hard has one row"),     LoadedHard.Num(), 1);
    TestEqual(TEXT("Blanks survive the round trip"), LoadedHard[0].Name, FString(TEXT(" MID X  ")));
    TestEqual(TEXT("Hard row 0 time"),      LoadedHard[0].TimeSeconds, 123.75f);

    IFileManager::Get().Delete(*IniPath);
    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoScoresIniIsPerCourseAndSorted,
    "PartyButtons.Octo.Scores.IniIsPerCourseAndSorted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoScoresIniIsPerCourseAndSorted::RunTest(const FString& Parameters)
{
    const FString IniPath = MakeScratchIniPath(TEXT("PerCourse"));
    IFileManager::Get().Delete(*IniPath);

    // Write ONLY the normal course, then load into a pair of default tables. The
    // absent hard section must leave the hard defaults alone — that is what makes
    // adding a course, or deleting one section by hand, safe without a migration.
    TArray<FOctoScoreEntry> OnlyNormal;
    OnlyNormal.Add(FOctoScoreEntry(OctoScores::SanitizeName(TEXT("SOLO")), 30.f));

    TestTrue(TEXT("SaveToIni reports success"),
        OctoScores::SaveToIni(OnlyNormal, TArray<FOctoScoreEntry>(), IniPath));

    TArray<FOctoScoreEntry> LoadedNormal = OctoScores::DefaultTable(EOctoCourse::Normal);
    TArray<FOctoScoreEntry> LoadedHard   = OctoScores::DefaultTable(EOctoCourse::Hard);
    const TArray<FOctoScoreEntry> HardDefaults = LoadedHard;

    OctoScores::LoadFromIni(IniPath, LoadedNormal, LoadedHard);

    TestEqual(TEXT("Normal came from the file"), LoadedNormal.Num(), 1);
    TestEqual(TEXT("Normal row 0 name"), LoadedNormal[0].Name, OctoScores::SanitizeName(TEXT("SOLO")));

    TestEqual(TEXT("Hard kept its seeded default"), LoadedHard.Num(), HardDefaults.Num());
    if (HardDefaults.Num() > 0 && LoadedHard.Num() > 0)
    {
        TestEqual(TEXT("Hard default name is untouched"), LoadedHard[0].Name, HardDefaults[0].Name);
    }

    // A hand-edited file can be out of order; the loader must not trust it, or
    // FindInsertRank (which assumes sorted input) would rank against nonsense.
    TArray<FOctoScoreEntry> Unsorted;
    Unsorted.Add(FOctoScoreEntry(OctoScores::SanitizeName(TEXT("SLOW")), 90.f));
    Unsorted.Add(FOctoScoreEntry(OctoScores::SanitizeName(TEXT("FAST")), 10.f));

    // SaveToIni writes in array order, so this really does produce a mis-ordered file.
    TestTrue(TEXT("SaveToIni reports success for the unsorted table"),
        OctoScores::SaveToIni(Unsorted, TArray<FOctoScoreEntry>(), IniPath));

    TArray<FOctoScoreEntry> Reloaded = OctoScores::DefaultTable(EOctoCourse::Normal);
    TArray<FOctoScoreEntry> IgnoredHard;
    OctoScores::LoadFromIni(IniPath, Reloaded, IgnoredHard);

    TestEqual(TEXT("Both rows loaded"), Reloaded.Num(), 2);
    if (Reloaded.Num() == 2)
    {
        TestEqual(TEXT("The loader sorted the file into ascending order"),
            Reloaded[0].Name, OctoScores::SanitizeName(TEXT("FAST")));
    }

    IFileManager::Get().Delete(*IniPath);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
