#include "PartyLoopAudioComponent.h"
#include "PartyAudioMath.h"
#include "PartyAudioSubsystem.h"
#include "PartySoundEvent.h"
#include "Components/AudioComponent.h"

UPartyLoopAudioComponent::UPartyLoopAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Ticks only while the drive or the voice is live — see TickComponent.
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPartyLoopAudioComponent::SetDrive(float NewDrive)
{
    Target = FMath::Clamp(NewDrive, 0.f, 1.f);
    if (Target > 0.f && !IsComponentTickEnabled())
    {
        SetComponentTickEnabled(true);
    }
}

bool UPartyLoopAudioComponent::IsVoicePlaying() const
{
    return Voice && Voice->IsPlaying();
}

void UPartyLoopAudioComponent::StopNow()
{
    Target = 0.f;
    Smoothed = 0.f;
    if (Voice)
    {
        Voice->Stop();
    }
    SetComponentTickEnabled(false);
}

void UPartyLoopAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopNow();
    Super::EndPlay(EndPlayReason);
}

void UPartyLoopAudioComponent::StartVoice()
{
    UPartyAudioSubsystem* Audio = UPartyAudioSubsystem::Get(this);
    FPartySoundPick Pick;
    if (!Audio || !Audio->ResolvePick(Event, Smoothed, FName(TEXT("Loop"), static_cast<int32>(GetUniqueID())), false, Pick))
    {
        return;
    }

    if (!Voice)
    {
        Voice = NewObject<UAudioComponent>(this, TEXT("LoopVoice"));
        Voice->bAutoActivate = false;
        Voice->bAutoDestroy = false;
        Voice->SetupAttachment(this);
        Voice->RegisterComponent();
    }

    Voice->SetSound(Pick.Sound);
    Voice->bAllowSpatialization = Event->bSpatialized;
    Voice->AttenuationSettings = Event->Attenuation;
    Voice->ConcurrencySet.Reset();
    if (Event->Concurrency)
    {
        Voice->ConcurrencySet.Add(Event->Concurrency);
    }

    JitterGain = Pick.JitterGain;
    JitterPitch = Pick.JitterPitch;
    ApplyShaping();
    Voice->Play(Pick.StartTime);
}

void UPartyLoopAudioComponent::ApplyShaping()
{
    if (!Voice || !Event)
    {
        return;
    }

    const PartyAudio::FIntensityShaping Shaping = Event->GetShaping();

    // Below the start threshold, fade to nothing so starting and stopping the
    // voice is never an audible step.
    const float Fade = StartThreshold > 0.f ? FMath::Clamp(Smoothed / StartThreshold, 0.f, 1.f) : 1.f;

    Voice->SetVolumeMultiplier(Shaping.EvaluateVolume(Smoothed) * PartyAudio::DbToLinear(Event->VolumeDb)
                               * JitterGain * Fade * VolumeMultiplier);
    Voice->SetPitchMultiplier(PartyAudio::SemitonesToPitch(Shaping.EvaluatePitchSemitones(Smoothed)) * JitterPitch);

    const float LowPassHz = Shaping.EvaluateLowPassHz(Smoothed);
    Voice->SetLowPassFilterEnabled(LowPassHz > 0.f);
    if (LowPassHz > 0.f)
    {
        Voice->SetLowPassFilterFrequency(LowPassHz);
    }
}

void UPartyLoopAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    Smoothed = PartyAudio::SmoothToward(Smoothed, Target, DeltaTime, AttackSeconds, ReleaseSeconds);

    const bool bPlaying = IsVoicePlaying();
    if (!bPlaying && Smoothed >= StartThreshold && Target >= StartThreshold)
    {
        StartVoice();
    }
    else if (bPlaying && Smoothed < StopThreshold && Target < StopThreshold)
    {
        Voice->Stop();
    }

    if (IsVoicePlaying())
    {
        ApplyShaping();
    }
    else if (Target <= 0.f && Smoothed < StopThreshold)
    {
        Smoothed = 0.f;
        SetComponentTickEnabled(false);
    }
}
