#include "OctoOdyssey/OctoScores.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"

namespace
{
    // Blank first — see GetAlphabet's comment. 37 glyphs.
    const TCHAR* const GAlphabet = TEXT(" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    /** The on-disk stand-in for a blank letter. Not in the alphabet, so it round-trips cleanly. */
    constexpr TCHAR GBlankOnDisk = TEXT('_');

    /** Key for row I inside a course's section. */
    FString EntryKey(int32 Index)
    {
        return FString::Printf(TEXT("Entry%d"), Index);
    }
}

const TCHAR* OctoScores::GetAlphabet()
{
    return GAlphabet;
}

int32 OctoScores::GetAlphabetLength()
{
    return FCString::Strlen(GAlphabet);
}

bool OctoScores::IsValidLetter(TCHAR Letter)
{
    // Guard the null: Strchr would happily "find" the terminator and report a hit.
    return Letter != TEXT('\0') && FCString::Strchr(GAlphabet, Letter) != nullptr;
}

TCHAR OctoScores::CycleLetter(TCHAR Current, int32 Direction)
{
    const int32 Num = GetAlphabetLength();

    const TCHAR* Found = IsValidLetter(Current) ? FCString::Strchr(GAlphabet, Current) : nullptr;
    const int32  Index = Found ? static_cast<int32>(Found - GAlphabet) : 0;

    // Double modulo, not a bare one: C++ % keeps the sign of the dividend, so a
    // negative Direction would otherwise index off the front of the string.
    const int32 Next = ((Index + Direction) % Num + Num) % Num;
    return GAlphabet[Next];
}

FString OctoScores::BlankName()
{
    return FString::ChrN(NameLength, TEXT(' '));
}

FString OctoScores::SanitizeName(const FString& In)
{
    FString Out;
    Out.Reserve(NameLength);

    for (int32 i = 0; i < NameLength; i++)
    {
        const TCHAR Raw = In.IsValidIndex(i) ? FChar::ToUpper(In[i]) : TEXT(' ');
        Out.AppendChar(IsValidLetter(Raw) ? Raw : TEXT(' '));
    }

    return Out;
}

int32 OctoScores::FindInsertRank(const TArray<FOctoScoreEntry>& Table, float TimeSeconds)
{
    // <= not <: a tie walks PAST the incumbent, so an equal time ranks below the
    // name that set it first (see the header).
    int32 Rank = 0;
    while (Rank < Table.Num() && Table[Rank].TimeSeconds <= TimeSeconds)
    {
        ++Rank;
    }

    return (Rank < NumSlots) ? Rank : INDEX_NONE;
}

int32 OctoScores::Insert(TArray<FOctoScoreEntry>& Table, const FOctoScoreEntry& Entry)
{
    const int32 Rank = FindInsertRank(Table, Entry.TimeSeconds);
    if (Rank == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    Table.Insert(FOctoScoreEntry(SanitizeName(Entry.Name), Entry.TimeSeconds), Rank);

    if (Table.Num() > NumSlots)
    {
        Table.SetNum(NumSlots);
    }

    return Rank;
}

void OctoScores::SortTable(TArray<FOctoScoreEntry>& Table)
{
    // StableSort so equal times keep the order they were entered in, which is the
    // same "first to set it stays ahead" rule FindInsertRank applies.
    Table.StableSort([](const FOctoScoreEntry& A, const FOctoScoreEntry& B)
    {
        return A.TimeSeconds < B.TimeSeconds;
    });
}

FString OctoScores::FormatTime(float Seconds)
{
    const float Clamped = FMath::Max(0.f, Seconds);

    // Round once, into hundredths, then decompose — rounding each field
    // separately produces things like "1:60.00".
    const int32 TotalHundredths = FMath::RoundToInt(Clamped * 100.f);

    const int32 Minutes    = TotalHundredths / 6000;
    const int32 Secs       = (TotalHundredths / 100) % 60;
    const int32 Hundredths = TotalHundredths % 100;

    return FString::Printf(TEXT("%d:%02d.%02d"), Minutes, Secs, Hundredths);
}

TArray<FOctoScoreEntry> OctoScores::DefaultTable(EOctoCourse Course)
{
    // One seeded score per course, so a fresh install still shows a populated
    // board and any finishing run has something to beat. Deliberately soft times:
    // the point of the seed is to be beaten, not to gate the board.
    if (Course == EOctoCourse::Hard)
    {
        return { FOctoScoreEntry(SanitizeName(TEXT("KRAKEN")), 150.f) };
    }

    return { FOctoScoreEntry(SanitizeName(TEXT("OCTOPUS")), 90.f) };
}

// ---- Persistence -----------------------------------------------------------

FString OctoScores::SectionForCourse(EOctoCourse Course)
{
    return FString::Printf(TEXT("OctoScores.%s"), OctoCourse::ToString(Course));
}

namespace
{
    /** "OCTOPUS " -> "OCTOPUS_,90.000000" */
    FString EncodeEntry(const FOctoScoreEntry& Entry)
    {
        FString Name = OctoScores::SanitizeName(Entry.Name);
        Name.ReplaceCharInline(TEXT(' '), GBlankOnDisk, ESearchCase::CaseSensitive);
        return FString::Printf(TEXT("%s,%f"), *Name, Entry.TimeSeconds);
    }

    /** Inverse of EncodeEntry. False if the line has no comma or no parsable time. */
    bool DecodeEntry(const FString& Encoded, FOctoScoreEntry& OutEntry)
    {
        FString NamePart;
        FString TimePart;

        // Split on the LAST comma: the alphabet contains no comma, so this is
        // unambiguous today, and it stays correct if one is ever added.
        if (!Encoded.Split(TEXT(","), &NamePart, &TimePart, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
        {
            return false;
        }

        TimePart.TrimStartAndEndInline();
        if (!TimePart.IsNumeric())
        {
            return false;
        }

        NamePart.ReplaceCharInline(GBlankOnDisk, TEXT(' '), ESearchCase::CaseSensitive);

        OutEntry.Name        = OctoScores::SanitizeName(NamePart);
        OutEntry.TimeSeconds = FMath::Max(0.f, FCString::Atof(*TimePart));
        return true;
    }

    /** Replace one course's section wholesale. */
    void WriteCourseSection(FConfigFile& File, EOctoCourse Course, const TArray<FOctoScoreEntry>& Table)
    {
        const FString Section = OctoScores::SectionForCourse(Course);

        // Empty the section first: a table that SHRANK (a reset, a hand-deleted
        // row) would otherwise leave the surplus Entry<n> keys behind, and the
        // next load would read them straight back in.
        File.Remove(Section);

        const int32 Count = FMath::Min(Table.Num(), OctoScores::NumSlots);
        for (int32 i = 0; i < Count; i++)
        {
            File.SetString(*Section, *EntryKey(i), *EncodeEntry(Table[i]));
        }
    }

    /** Read one course's section. Leaves InOutTable untouched when the section is absent. */
    int32 ReadCourseSection(const FConfigFile& File, EOctoCourse Course, TArray<FOctoScoreEntry>& InOutTable)
    {
        // FindSection, not Find: FConfigFile inherits its map PRIVATELY, so the
        // TMap interface is only reachable through the accessors it re-exposes.
        const FString Section = OctoScores::SectionForCourse(Course);
        if (!File.FindSection(Section))
        {
            return 0;
        }

        TArray<FOctoScoreEntry> Loaded;

        // Stop at the first missing index rather than scanning the whole range:
        // the keys are written contiguously, so a gap means the file was edited
        // down and everything past it is stale.
        for (int32 i = 0; i < OctoScores::NumSlots; i++)
        {
            FString Encoded;
            if (!File.GetString(*Section, *EntryKey(i), Encoded))
            {
                break;
            }

            FOctoScoreEntry Entry;
            if (DecodeEntry(Encoded, Entry))
            {
                Loaded.Add(Entry);
            }
        }

        // A section that exists but yields nothing usable is treated as absent, so
        // a mangled file falls back to the seeded defaults instead of an empty board.
        if (Loaded.IsEmpty())
        {
            return 0;
        }

        OctoScores::SortTable(Loaded); // the file is hand-editable — never trust its order
        InOutTable = MoveTemp(Loaded);
        return InOutTable.Num();
    }
}

bool OctoScores::SaveToIni(
    const TArray<FOctoScoreEntry>& NormalTable,
    const TArray<FOctoScoreEntry>& HardTable,
    const FString&                 IniPath)
{
    if (IniPath.IsEmpty())
    {
        return false;
    }

    // Local, not GConfig — see the header.
    FConfigFile File;
    File.Read(IniPath); // preserves any unrelated sections a human added

    WriteCourseSection(File, EOctoCourse::Normal, NormalTable);
    WriteCourseSection(File, EOctoCourse::Hard,   HardTable);

    // Explicit: FConfigFile::Write silently skips a non-dirty file AND returns
    // true, so a stale Dirty flag would look exactly like a successful save.
    File.Dirty = true;

    return File.Write(IniPath);
}

int32 OctoScores::LoadFromIni(
    const FString&           IniPath,
    TArray<FOctoScoreEntry>& InOutNormalTable,
    TArray<FOctoScoreEntry>& InOutHardTable)
{
    if (IniPath.IsEmpty() || !IFileManager::Get().FileExists(*IniPath))
    {
        return 0;
    }

    FConfigFile File;
    File.Read(IniPath);

    int32 NumLoaded = 0;
    NumLoaded += ReadCourseSection(File, EOctoCourse::Normal, InOutNormalTable);
    NumLoaded += ReadCourseSection(File, EOctoCourse::Hard,   InOutHardTable);

    return NumLoaded;
}
