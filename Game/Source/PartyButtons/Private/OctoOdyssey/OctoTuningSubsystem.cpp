#include "OctoOdyssey/OctoTuningSubsystem.h"
#include "PartyButtons.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

/*static*/ UOctoTuningSubsystem* UOctoTuningSubsystem::Get(const UObject* WorldContext)
{
    if (!WorldContext || !GEngine)
    {
        return nullptr;
    }

    // ReturnNull, not LogAndReturnNull: AOctoPawn::OnConstruction calls this from
    // the editor viewport (and from the CDO) where there is legitimately no world,
    // and a warning per construction-script run would be pure noise.
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
    if (!World)
    {
        return nullptr;
    }

    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UOctoTuningSubsystem>() : nullptr;
}

/*static*/ FString UOctoTuningSubsystem::GetIniPath()
{
    // Config/, not Saved/Config/: this is a file you are meant to find, read,
    // hand-edit and optionally commit — it is the working record of a tuning
    // pass, not engine scratch state.
    //
    // Absolute, so the path in the logs is one you can paste into a shell. The
    // relative form ProjectConfigDir returns resolves fine but reads as
    // "../../../../../../Users/...", which helps nobody.
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("OctoTuning.ini"));
}

void UOctoTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadFromIni();
}

void UOctoTuningSubsystem::LoadFromIni()
{
    // Start from the shipping defaults every time, so a field the file doesn't
    // mention is a field that tracks its C++ default.
    Tuning = FOctoTuning();

    const FString IniPath = GetIniPath();
    if (!IFileManager::Get().FileExists(*IniPath))
    {
        UE_LOG(LogPartyButtons, Log,
            TEXT("UOctoTuningSubsystem: no %s — using the FOctoTuning defaults."), *IniPath);
        return;
    }

    FString Overridden;
    const int32 NumOverridden = OctoTuning::LoadFromIni(Tuning, IniPath, &Overridden);

    // Logged in full, by name: an on-disk override that silently outranks a C++
    // default is exactly the kind of thing that wastes an afternoon otherwise.
    UE_LOG(LogPartyButtons, Log,
        TEXT("UOctoTuningSubsystem: loaded %s — %d value(s) override the defaults:%s"),
        *IniPath, NumOverridden, NumOverridden > 0 ? *Overridden : TEXT(" (none)"));
}

void UOctoTuningSubsystem::SaveToIni() const
{
    const FString IniPath = GetIniPath();

    if (OctoTuning::SaveToIni(Tuning, IniPath))
    {
        UE_LOG(LogPartyButtons, Log, TEXT("UOctoTuningSubsystem: saved %s"), *IniPath);
        return;
    }

    // Never downgrade this to a Log line. A save that quietly fails is
    // indistinguishable from tuning that "just doesn't persist", and that is
    // precisely how the GConfig version of this code hid its own bug.
    UE_LOG(LogPartyButtons, Warning,
        TEXT("UOctoTuningSubsystem: FAILED to save %s — tuning will not persist."), *IniPath);
}

void UOctoTuningSubsystem::ResetTuning()
{
    Tuning = FOctoTuning();

    // Delete rather than rewrite the defaults: the point of a reset is to stop
    // the file overriding anything, so a C++ default changed later takes effect.
    const FString IniPath = GetIniPath();
    if (IFileManager::Get().FileExists(*IniPath))
    {
        IFileManager::Get().Delete(*IniPath);
    }

    UE_LOG(LogPartyButtons, Log,
        TEXT("UOctoTuningSubsystem: reset to FOctoTuning defaults and deleted %s"), *IniPath);
}
