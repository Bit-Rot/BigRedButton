#include "JukeBox.h"
#include "PartyButtons.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

AJukeBox::AJukeBox()
{
    // Can tick, starts off: RefreshTickEnabled turns it on only while an ease is running.
    PrimaryActorTick.bCanEverTick          = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AJukeBox::BeginPlay()
{
    Super::BeginPlay();

    EnsureRuntime();

    for (int32 i = 0; i < Tracks.Num(); i++)
    {
        if (Tracks[i].bAutoPlay)
        {
            PlayTrack(Tracks[i].Name);
        }
    }
}

void AJukeBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (UAudioComponent* Component : TrackComponents)
    {
        if (Component)
        {
            Component->Stop();
        }
    }

    Super::EndPlay(EndPlayReason);
}

void AJukeBox::EnsureRuntime()
{
    // Grow-only: tracks added by AddTrack after the first call get seeded
    // without disturbing the volumes (and eases) of the ones already running.
    for (int32 i = Volumes.Num(); i < Tracks.Num(); i++)
    {
        Volumes.Emplace(Tracks[i].Volume);
    }

    TrackComponents.SetNum(Tracks.Num());
}

void AJukeBox::AddTrack(FName TrackName, USoundBase* Sound, float Volume, bool bAutoPlay)
{
    Modify(); // undo + dirty the level when called from the editor

    FJukeBoxTrack NewTrack;
    NewTrack.Name      = TrackName;
    NewTrack.Sound     = Sound;
    NewTrack.Volume    = FMath::Max(0.f, Volume);
    NewTrack.bAutoPlay = bAutoPlay;

    int32 Index = Tracks.IndexOfByPredicate([TrackName](const FJukeBoxTrack& Track) { return Track.Name == TrackName; });
    if (Index == INDEX_NONE)
    {
        Index = Tracks.Add(NewTrack);
    }
    else
    {
        Tracks[Index] = NewTrack;
    }

    // Editor-time calls stop here: the runtime half is built on first use in play.
    if (!HasActorBegunPlay()) { return; }

    EnsureRuntime();

    // A replaced track starts over: new sound, new starting volume.
    if (UAudioComponent* Old = TrackComponents[Index])
    {
        Old->Stop();
        Old->DestroyComponent();
        TrackComponents[Index] = nullptr;
    }
    Volumes[Index].SetImmediate(NewTrack.Volume);
    RefreshTickEnabled();

    if (bAutoPlay)
    {
        PlayTrack(TrackName);
    }
}

int32 AJukeBox::FindTrack(FName TrackName) const
{
    const int32 Index = Tracks.IndexOfByPredicate([TrackName](const FJukeBoxTrack& Track)
    {
        return Track.Name == TrackName;
    });

    if (Index == INDEX_NONE)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("AJukeBox '%s': no track named '%s'."),
            *GetActorNameOrLabel(), *TrackName.ToString());
    }

    return Index;
}

UAudioComponent* AJukeBox::GetOrCreateComponent(int32 Index)
{
    if (UAudioComponent* Existing = TrackComponents[Index])
    {
        return Existing;
    }

    const FJukeBoxTrack& Track = Tracks[Index];
    if (!Track.Sound)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("AJukeBox '%s': track '%s' has no Sound assigned."),
            *GetActorNameOrLabel(), *Track.Name.ToString());
        return nullptr;
    }

    UAudioComponent* Component = NewObject<UAudioComponent>(this);
    Component->SetupAttachment(RootComponent);
    Component->SetSound(Track.Sound);
    Component->bAutoActivate        = false;
    Component->bAutoDestroy         = false;
    Component->bAllowSpatialization = false; // music, not a sound in the world
    Component->RegisterComponent();

    TrackComponents[Index] = Component;
    return Component;
}

void AJukeBox::ApplyVolume(int32 Index)
{
    if (UAudioComponent* Component = TrackComponents[Index])
    {
        Component->SetVolumeMultiplier(Volumes[Index].GetCurrent());
    }
}

void AJukeBox::PlayTrack(FName TrackName)
{
    EnsureRuntime();

    const int32 Index = FindTrack(TrackName);
    if (Index == INDEX_NONE) { return; }

    UAudioComponent* Component = GetOrCreateComponent(Index);
    if (!Component || Component->IsPlaying()) { return; }

    // Volume first, so the first buffer out is already at whatever level was
    // asked for before the track started — the menu's quiet level, typically.
    ApplyVolume(Index);
    Component->Play();
}

void AJukeBox::StopTrack(FName TrackName)
{
    EnsureRuntime();

    const int32 Index = FindTrack(TrackName);
    if (Index == INDEX_NONE) { return; }

    if (UAudioComponent* Component = TrackComponents[Index])
    {
        Component->Stop();
    }
}

bool AJukeBox::IsTrackPlaying(FName TrackName) const
{
    const int32 Index = FindTrack(TrackName);
    if (Index == INDEX_NONE || !TrackComponents.IsValidIndex(Index)) { return false; }

    const UAudioComponent* Component = TrackComponents[Index];
    return Component && Component->IsPlaying();
}

bool AJukeBox::HasTrack(FName TrackName) const
{
    // Not FindTrack: asking is not a mistake, so it must not warn.
    return Tracks.ContainsByPredicate([TrackName](const FJukeBoxTrack& Track) { return Track.Name == TrackName; });
}

void AJukeBox::SetTrackVolume(FName TrackName, float Volume)
{
    EnsureRuntime();

    const int32 Index = FindTrack(TrackName);
    if (Index == INDEX_NONE) { return; }

    Volumes[Index].SetImmediate(Volume);
    ApplyVolume(Index);
    RefreshTickEnabled();
}

void AJukeBox::EaseTrackVolume(FName TrackName, float TargetVolume, float Seconds)
{
    EnsureRuntime();

    const int32 Index = FindTrack(TrackName);
    if (Index == INDEX_NONE) { return; }

    Volumes[Index].EaseTo(TargetVolume, Seconds);
    ApplyVolume(Index);
    RefreshTickEnabled();
}

float AJukeBox::GetTrackVolume(FName TrackName) const
{
    const int32 Index = FindTrack(TrackName);
    if (Index == INDEX_NONE) { return 0.f; }

    // Before EnsureRuntime has run nothing can have moved it yet.
    return Volumes.IsValidIndex(Index) ? Volumes[Index].GetCurrent() : Tracks[Index].Volume;
}

void AJukeBox::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    for (int32 i = 0; i < Volumes.Num(); i++)
    {
        if (Volumes[i].IsEasing())
        {
            Volumes[i].Advance(DeltaSeconds);
            ApplyVolume(i);
        }
    }

    RefreshTickEnabled();
}

void AJukeBox::RefreshTickEnabled()
{
    const bool bAnyEasing = Volumes.ContainsByPredicate([](const FJukeBoxVolumeEase& V) { return V.IsEasing(); });
    SetActorTickEnabled(bAnyEasing);
}
