#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PartyAudioMath.h"
#include "PartyAudioSubsystem.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;
class UPartySoundEvent;

/** One resolved play of a UPartySoundEvent: which recording, and how it is shaped. */
struct PARTYAUDIO_API FPartySoundPick
{
    USoundBase* Sound = nullptr;
    int32 VariantIndex = INDEX_NONE;
    float Volume    = 1.f; ///< Linear, everything folded in: VolumeDb, intensity curve, jitter.
    float Pitch     = 1.f; ///< Multiplier, intensity curve + jitter.
    float LowPassHz = 0.f; ///< 0 = no filter.
    float StartTime = 0.f;
    float JitterGain  = 1.f; ///< The random part of Volume alone, linear. For voices that re-shape every frame.
    float JitterPitch = 1.f; ///< The random part of Pitch alone, as a multiplier.
};

/**
 * UPartyAudioSubsystem
 *
 * The one place sound events are turned into sound. Gameplay (or one of the
 * emitter components) calls PlayEvent with an event, a location and an
 * intensity; this resolves the variant, applies the shaping, respects the
 * per-source cooldown and spawns the voice.
 *
 * Per-world on purpose: its state (shuffle bags, cooldowns) is about what has
 * been heard in THIS level, and it should all be forgotten on travel.
 *
 * State is keyed by (event, SourceKey). SourceKey separates independent emitters
 * of the same event — eight octopus arms each plant with their own cooldown and
 * their own shuffle bag, rather than one arm's plant muting the next. NAME_None
 * is a fine key when there is only one emitter.
 *
 * Debugging: `PartyAudio.Debug 1` draws a label in the world at every play
 * (event, intensity, variant) and logs cooldown suppressions.
 */
UCLASS()
class PARTYAUDIO_API UPartyAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The subsystem for WorldContext's world, or null (no world, or a world type we don't serve). */
    static UPartyAudioSubsystem* Get(const UObject* WorldContext);

    /**
     * Play Event at Location, shaped by Intensity (0..1). Non-spatialised events
     * ignore Location and play 2D. Returns the voice, or null if nothing played
     * (no event, nothing playable, on cooldown, or culled by concurrency).
     */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    UAudioComponent* PlayEvent(UPartySoundEvent* Event, FVector Location, float Intensity = 1.f,
                               FName SourceKey = NAME_None, float VolumeMultiplier = 1.f);

    /** PlayEvent without a location — UI, stingers, beds. */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    UAudioComponent* PlayEvent2D(UPartySoundEvent* Event, float Intensity = 1.f,
                                 FName SourceKey = NAME_None, float VolumeMultiplier = 1.f);

    /** PlayEvent that follows AttachTo as it moves — for a one-shot on a fast mover. */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    UAudioComponent* PlayEventAttached(UPartySoundEvent* Event, USceneComponent* AttachTo, float Intensity = 1.f,
                                       FName SourceKey = NAME_None, float VolumeMultiplier = 1.f);

    /**
     * Resolve a play without spawning anything. False when there is nothing to
     * play or (bRespectCooldown) the source is still cooling down. Used by
     * UPartyLoopAudioComponent, which owns its own long-lived voice.
     */
    bool ResolvePick(const UPartySoundEvent* Event, float Intensity, FName SourceKey, bool bRespectCooldown, FPartySoundPick& OutPick);

    /**
     * Measure sound distances from Focus instead of from the camera, while
     * panning still comes from the camera. This is what a side-on or top-down
     * game wants: the camera sits far from the action, so without it every
     * spatialised sound is attenuated by the camera distance and the player's
     * own character sounds distant. Null clears it. Applies to every local
     * player controller; call again when the focus actor is replaced.
     */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void SetAttenuationFocus(USceneComponent* Focus);

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    struct FEventState
    {
        PartyAudio::FShuffleBag Bag;
        PartyAudio::FCooldownGate Cooldown;
    };

    FEventState& GetState(const UPartySoundEvent* Event, FName SourceKey);

    /** Apply the pick's filter to a freshly spawned voice and emit the debug label. */
    void FinishVoice(UAudioComponent* Voice, const UPartySoundEvent* Event, const FPartySoundPick& Pick,
                     float Intensity, const FVector& DebugLocation) const;

    TMap<TPair<TObjectKey<UPartySoundEvent>, FName>, FEventState> States;

    FRandomStream Stream;

    TArray<PartyAudio::FVariantBand> ScratchBands;
    TArray<int32> ScratchCandidates;
};
