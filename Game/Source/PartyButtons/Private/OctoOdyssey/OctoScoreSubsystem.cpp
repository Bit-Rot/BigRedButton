#include "OctoOdyssey/OctoScoreSubsystem.h"
#include "PartyButtons.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

/*static*/ UOctoScoreSubsystem* UOctoScoreSubsystem::Get(const UObject* WorldContext)
{
    if (!WorldContext || !GEngine)
    {
        return nullptr;
    }

    // ReturnNull, not LogAndReturnNull — matching UOctoTuningSubsystem::Get. The
    // HUD asks for this every frame, so a per-frame warning in any context
    // without a world would drown the log.
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
    if (!World)
    {
        return nullptr;
    }

    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UOctoScoreSubsystem>() : nullptr;
}

/*static*/ FString UOctoScoreSubsystem::GetIniPath()
{
    // Config/, alongside OctoTuning.ini, and absolute for the same two reasons
    // given there: this is a file you are meant to find and hand-edit, and a path
    // in the log should be one you can paste into a shell.
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("OctoScores.ini"));
}

void UOctoScoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadFromIni();
}

TArray<FOctoScoreEntry>& UOctoScoreSubsystem::TableFor(EOctoCourse Course)
{
    return Course == EOctoCourse::Hard ? HardTable : NormalTable;
}

const TArray<FOctoScoreEntry>& UOctoScoreSubsystem::GetTable(EOctoCourse Course) const
{
    return Course == EOctoCourse::Hard ? HardTable : NormalTable;
}

int32 UOctoScoreSubsystem::FindInsertRank(EOctoCourse Course, float TimeSeconds) const
{
    return OctoScores::FindInsertRank(GetTable(Course), TimeSeconds);
}

int32 UOctoScoreSubsystem::SubmitScore(EOctoCourse Course, const FOctoScoreEntry& Entry)
{
    const int32 Rank = OctoScores::Insert(TableFor(Course), Entry);

    if (Rank == INDEX_NONE)
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("UOctoScoreSubsystem: %s run of %s missed the top %d — nothing stored."),
            OctoCourse::ToString(Course), *OctoScores::FormatTime(Entry.TimeSeconds), OctoScores::NumSlots);
        return INDEX_NONE;
    }

    UE_LOG(LogPartyButtons, Log, TEXT("UOctoScoreSubsystem: %s '%s' %s entered at rank %d."),
        OctoCourse::ToString(Course), *OctoScores::SanitizeName(Entry.Name),
        *OctoScores::FormatTime(Entry.TimeSeconds), Rank + 1);

    // Written immediately, not at shutdown — see the class comment.
    SaveToIni();

    return Rank;
}

void UOctoScoreSubsystem::LoadFromIni()
{
    // Start from the seeded defaults every time, so a course the file doesn't
    // mention still shows a populated board.
    NormalTable = OctoScores::DefaultTable(EOctoCourse::Normal);
    HardTable   = OctoScores::DefaultTable(EOctoCourse::Hard);

    const FString IniPath = GetIniPath();
    if (!IFileManager::Get().FileExists(*IniPath))
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("UOctoScoreSubsystem: no %s — starting from the seeded default tables."), *IniPath);
        return;
    }

    const int32 NumLoaded = OctoScores::LoadFromIni(IniPath, NormalTable, HardTable);

    UE_LOG(LogPartyButtons, Log,
        TEXT("UOctoScoreSubsystem: loaded %s — %d entr%s (%d normal, %d hard)."),
        *IniPath, NumLoaded, NumLoaded == 1 ? TEXT("y") : TEXT("ies"),
        NormalTable.Num(), HardTable.Num());
}

void UOctoScoreSubsystem::SaveToIni() const
{
    const FString IniPath = GetIniPath();

    if (OctoScores::SaveToIni(NormalTable, HardTable, IniPath))
    {
        UE_LOG(LogPartyButtons, Log, TEXT("UOctoScoreSubsystem: saved %s"), *IniPath);
        return;
    }

    // Never a Log line. A silently failing save is indistinguishable from scores
    // that "just don't persist" — the exact failure mode the local-FConfigFile
    // pattern exists to avoid (see OctoScores.h).
    UE_LOG(LogPartyButtons, Warning,
        TEXT("UOctoScoreSubsystem: FAILED to save %s — scores will not persist."), *IniPath);
}

void UOctoScoreSubsystem::ResetTables()
{
    NormalTable = OctoScores::DefaultTable(EOctoCourse::Normal);
    HardTable   = OctoScores::DefaultTable(EOctoCourse::Hard);

    // Delete rather than rewrite the seeds: an absent file is the honest
    // "nothing has been scored here yet" state, and it keeps the seeded defaults
    // tracking their C++ values if those ever change.
    const FString IniPath = GetIniPath();
    if (IFileManager::Get().FileExists(*IniPath))
    {
        IFileManager::Get().Delete(*IniPath);
    }

    UE_LOG(LogPartyButtons, Log,
        TEXT("UOctoScoreSubsystem: reset both tables to defaults and deleted %s"), *IniPath);
}
