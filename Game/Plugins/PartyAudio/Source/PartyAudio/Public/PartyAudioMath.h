#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

/**
 * PartyAudio — the world-free half of the audio framework.
 *
 * Everything here is plain maths on plain data so it can be unit tested with no
 * audio device, no world and no assets (PartyButtons.Audio.Framework.*). The
 * subsystem and components are thin shells that feed game state in and push the
 * answers into UAudioComponents.
 */
namespace PartyAudio
{
    /** The part of a sound variant the selection logic cares about. */
    struct FVariantBand
    {
        float Weight       = 1.f;
        float MinIntensity = 0.f;
        float MaxIntensity = 1.f;
    };

    /**
     * Indices of the variants that may play at Intensity: weight > 0 and Intensity
     * inside [Min, Max]. When no band contains Intensity (a gap in the authoring),
     * falls back to the variants whose band is nearest, so an event with content
     * never goes silent because of a band typo.
     */
    PARTYAUDIO_API void FilterByIntensity(TConstArrayView<FVariantBand> Bands, float Intensity, TArray<int32>& OutCandidates);

    /** Weighted pick over Candidates (indices into Bands). Roll01 in [0,1). INDEX_NONE if Candidates is empty. */
    PARTYAUDIO_API int32 PickWeighted(TConstArrayView<FVariantBand> Bands, TConstArrayView<int32> Candidates, float Roll01);

    /**
     * Weighted pick that avoids Previous whenever there is any alternative.
     * The cheap cure for the "same sample twice" machine-gun effect.
     */
    PARTYAUDIO_API int32 PickNoRepeat(TConstArrayView<FVariantBand> Bands, TConstArrayView<int32> Candidates, int32 Previous, FRandomStream& Stream);

    /**
     * Shuffle bag: every candidate plays once, in random order, before any plays
     * again — and the first draw of a refill never repeats the last draw of the
     * previous bag. Weights are ignored (a bag is uniform by definition); a zero
     * weight still excludes a variant because FilterByIntensity drops it.
     *
     * Tolerates the candidate set changing between draws (a harder hit selects a
     * different intensity band): entries no longer allowed are discarded, and the
     * bag refills from the current candidates when it runs dry.
     */
    struct PARTYAUDIO_API FShuffleBag
    {
        int32 Draw(TConstArrayView<int32> Candidates, FRandomStream& Stream);

        TArray<int32> Remaining;
        int32 Last = INDEX_NONE;
    };

    /** Minimum-interval gate for one event on one source. */
    struct PARTYAUDIO_API FCooldownGate
    {
        /** True (and arms the gate) if at least MinInterval has passed since the last pass. */
        bool TryPass(double Now, float MinInterval);

        double LastPassTime = -1.0e9;
    };

    /**
     * How an event's loudness, pitch and brightness follow intensity. All values
     * are authored on UPartySoundEvent; this is the evaluation.
     */
    struct PARTYAUDIO_API FIntensityShaping
    {
        float VolumeAtMin   = 0.25f; ///< Linear gain at intensity 0.
        float VolumeAtMax   = 1.f;   ///< Linear gain at intensity 1.
        float VolumeExponent = 1.f;  ///< >1 keeps soft hits softer for longer (more dynamic range).
        float PitchAtMinSemitones = 0.f;
        float PitchAtMaxSemitones = 0.f;
        float LowPassAtMinHz = 0.f;  ///< 0 disables. Otherwise the cutoff at intensity 0, opening to fully bright at 1.

        float EvaluateVolume(float Intensity) const;
        float EvaluatePitchSemitones(float Intensity) const;

        /** The low-pass cutoff for Intensity, or 0 when the event does not filter. */
        float EvaluateLowPassHz(float Intensity) const;
    };

    /** The frequency EvaluateLowPassHz opens up to at full intensity — effectively "no filter". */
    constexpr float FullyOpenLowPassHz = 20000.f;

    PARTYAUDIO_API float DbToLinear(float Db);
    PARTYAUDIO_API float SemitonesToPitch(float Semitones);

    /**
     * Map a raw physical quantity (a speed change, an angular speed) to 0..1
     * between Floor and Ceiling. Below Floor is 0 — the caller treats that as
     * "too soft to hear". Ceiling <= Floor degenerates to a step at Floor.
     */
    PARTYAUDIO_API float NormalizeIntensity(float Value, float Floor, float Ceiling);

    /**
     * One-pole smoothing toward Target with separate time constants for rising
     * (Attack) and falling (Release). Frame-rate independent. A time constant of
     * 0 snaps.
     */
    PARTYAUDIO_API float SmoothToward(float Current, float Target, float DeltaSeconds, float AttackSeconds, float ReleaseSeconds);
}
