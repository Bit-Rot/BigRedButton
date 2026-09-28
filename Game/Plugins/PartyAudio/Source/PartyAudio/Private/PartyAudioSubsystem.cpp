#include "PartyAudioSubsystem.h"
#include "PartyAudio.h"
#include "PartySoundEvent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
    int32 GPartyAudioDebug = 0;
    FAutoConsoleVariableRef CVarPartyAudioDebug(
        TEXT("PartyAudio.Debug"),
        GPartyAudioDebug,
        TEXT("1 = draw a label at every PartyAudio event play and log cooldown suppressions."),
        ECVF_Cheat);
}

/*static*/ UPartyAudioSubsystem* UPartyAudioSubsystem::Get(const UObject* WorldContext)
{
    if (!WorldContext || !GEngine)
    {
        return nullptr;
    }
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
    return World ? World->GetSubsystem<UPartyAudioSubsystem>() : nullptr;
}

bool UPartyAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPartyAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Stream.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
}

UPartyAudioSubsystem::FEventState& UPartyAudioSubsystem::GetState(const UPartySoundEvent* Event, FName SourceKey)
{
    return States.FindOrAdd(TPair<TObjectKey<UPartySoundEvent>, FName>(Event, SourceKey));
}

bool UPartyAudioSubsystem::ResolvePick(const UPartySoundEvent* Event, float Intensity, FName SourceKey,
                                       bool bRespectCooldown, FPartySoundPick& OutPick)
{
    if (!Event)
    {
        return false;
    }

    Intensity = FMath::Clamp(Intensity, 0.f, 1.f);
    FEventState& State = GetState(Event, SourceKey);

    if (bRespectCooldown)
    {
        const UWorld* World = GetWorld();
        const double Now = World ? World->GetAudioTimeSeconds() : FPlatformTime::Seconds();
        if (!State.Cooldown.TryPass(Now, Event->MinRetriggerSeconds))
        {
            UE_CLOG(GPartyAudioDebug != 0, LogPartyAudio, Log, TEXT("%s [%s] suppressed by cooldown"),
                    *Event->GetName(), *SourceKey.ToString());
            return false;
        }
    }

    Event->GetBands(ScratchBands);
    PartyAudio::FilterByIntensity(ScratchBands, Intensity, ScratchCandidates);

    int32 Index = INDEX_NONE;
    switch (Event->Selection)
    {
    case EPartySoundSelection::Random:
        Index = PartyAudio::PickWeighted(ScratchBands, ScratchCandidates, Stream.GetFraction());
        break;
    case EPartySoundSelection::NoRepeat:
        Index = PartyAudio::PickNoRepeat(ScratchBands, ScratchCandidates, State.Bag.Last, Stream);
        State.Bag.Last = Index;
        break;
    case EPartySoundSelection::ShuffleBag:
        Index = State.Bag.Draw(ScratchCandidates, Stream);
        break;
    }

    if (!Event->Variants.IsValidIndex(Index) || !Event->Variants[Index].Sound)
    {
        return false;
    }

    const PartyAudio::FIntensityShaping Shaping = Event->GetShaping();
    const float JitterDb = Stream.FRandRange(-Event->VolumeJitterDb, Event->VolumeJitterDb);
    const float JitterSt = Stream.FRandRange(-Event->PitchJitterSemitones, Event->PitchJitterSemitones);

    OutPick.Sound        = Event->Variants[Index].Sound;
    OutPick.VariantIndex = Index;
    OutPick.Volume       = Shaping.EvaluateVolume(Intensity) * PartyAudio::DbToLinear(Event->VolumeDb + JitterDb);
    OutPick.Pitch        = PartyAudio::SemitonesToPitch(Shaping.EvaluatePitchSemitones(Intensity) + JitterSt);
    OutPick.LowPassHz    = Shaping.EvaluateLowPassHz(Intensity);
    OutPick.StartTime    = 0.f;
    OutPick.JitterGain   = PartyAudio::DbToLinear(JitterDb);
    OutPick.JitterPitch  = PartyAudio::SemitonesToPitch(JitterSt);
    if (Event->bRandomStartTime && OutPick.Sound->GetDuration() > 0.f && OutPick.Sound->GetDuration() < INDEFINITELY_LOOPING_DURATION)
    {
        OutPick.StartTime = Stream.FRandRange(0.f, OutPick.Sound->GetDuration());
    }
    return true;
}

void UPartyAudioSubsystem::FinishVoice(UAudioComponent* Voice, const UPartySoundEvent* Event, const FPartySoundPick& Pick,
                                       float Intensity, const FVector& DebugLocation) const
{
    if (Voice && Pick.LowPassHz > 0.f)
    {
        Voice->SetLowPassFilterEnabled(true);
        Voice->SetLowPassFilterFrequency(Pick.LowPassHz);
    }

    UE_CLOG(GPartyAudioDebug != 0, LogPartyAudio, Log, TEXT("play %s  i=%.2f  variant %d  vol %.2f  pitch %.2f  lpf %.0f%s"),
            *Event->GetName(), Intensity, Pick.VariantIndex, Pick.Volume, Pick.Pitch, Pick.LowPassHz,
            Voice ? TEXT("") : TEXT("  (culled)"));

#if ENABLE_DRAW_DEBUG
    if (GPartyAudioDebug != 0)
    {
        const FString Label = FString::Printf(TEXT("%s  i=%.2f  v%d  %.2fx%s"),
            *Event->GetName(), Intensity, Pick.VariantIndex, Pick.Volume, Voice ? TEXT("") : TEXT("  CULLED"));
        DrawDebugString(GetWorld(), DebugLocation, Label, nullptr, Voice ? FColor::Cyan : FColor::Red, 1.5f, true, 1.1f);
    }
#endif
}

UAudioComponent* UPartyAudioSubsystem::PlayEvent(UPartySoundEvent* Event, FVector Location, float Intensity,
                                                 FName SourceKey, float VolumeMultiplier)
{
    if (Event && !Event->bSpatialized)
    {
        return PlayEvent2D(Event, Intensity, SourceKey, VolumeMultiplier);
    }

    FPartySoundPick Pick;
    if (!ResolvePick(Event, Intensity, SourceKey, true, Pick))
    {
        return nullptr;
    }

    UAudioComponent* Voice = UGameplayStatics::SpawnSoundAtLocation(
        this, Pick.Sound, Location, FRotator::ZeroRotator, Pick.Volume * VolumeMultiplier, Pick.Pitch,
        Pick.StartTime, Event->Attenuation, Event->Concurrency, true);
    FinishVoice(Voice, Event, Pick, Intensity, Location);
    return Voice;
}

UAudioComponent* UPartyAudioSubsystem::PlayEvent2D(UPartySoundEvent* Event, float Intensity, FName SourceKey, float VolumeMultiplier)
{
    FPartySoundPick Pick;
    if (!ResolvePick(Event, Intensity, SourceKey, true, Pick))
    {
        return nullptr;
    }

    UAudioComponent* Voice = UGameplayStatics::SpawnSound2D(
        this, Pick.Sound, Pick.Volume * VolumeMultiplier, Pick.Pitch, Pick.StartTime, Event->Concurrency, false, true);

    FVector DebugLocation = FVector::ZeroVector;
    if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        FRotator Unused;
        PC->GetPlayerViewPoint(DebugLocation, Unused);
        DebugLocation += Unused.Vector() * 500.f;
    }
    FinishVoice(Voice, Event, Pick, Intensity, DebugLocation);
    return Voice;
}

UAudioComponent* UPartyAudioSubsystem::PlayEventAttached(UPartySoundEvent* Event, USceneComponent* AttachTo, float Intensity,
                                                         FName SourceKey, float VolumeMultiplier)
{
    if (!AttachTo || (Event && !Event->bSpatialized))
    {
        return PlayEvent2D(Event, Intensity, SourceKey, VolumeMultiplier);
    }

    FPartySoundPick Pick;
    if (!ResolvePick(Event, Intensity, SourceKey, true, Pick))
    {
        return nullptr;
    }

    UAudioComponent* Voice = UGameplayStatics::SpawnSoundAttached(
        Pick.Sound, AttachTo, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset, true, Pick.Volume * VolumeMultiplier, Pick.Pitch,
        Pick.StartTime, Event->Attenuation, Event->Concurrency, true);
    FinishVoice(Voice, Event, Pick, Intensity, AttachTo->GetComponentLocation());
    return Voice;
}

void UPartyAudioSubsystem::SetAttenuationFocus(USceneComponent* Focus)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController())
        {
            continue;
        }
        if (Focus)
        {
            PC->SetAudioListenerAttenuationOverride(Focus, FVector::ZeroVector);
        }
        else
        {
            PC->ClearAudioListenerAttenuationOverride();
        }
    }
}
