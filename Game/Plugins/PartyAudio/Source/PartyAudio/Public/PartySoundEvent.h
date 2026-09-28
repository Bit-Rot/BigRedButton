#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PartyAudioMath.h"
#include "PartySoundEvent.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

/** How UPartySoundEvent chooses among the variants allowed at a given intensity. */
UENUM(BlueprintType)
enum class EPartySoundSelection : uint8
{
    /** Weighted random; repeats allowed. */
    Random,
    /** Weighted random, never the same variant twice in a row (when there is a choice). */
    NoRepeat,
    /** Every variant once, shuffled, before any repeats. Ignores weights. The default. */
    ShuffleBag,
};

/** One recording an event may play. */
USTRUCT(BlueprintType)
struct PARTYAUDIO_API FPartySoundVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    TObjectPtr<USoundBase> Sound;

    /** Relative likelihood under Random/NoRepeat. 0 disables the variant entirely. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.0"))
    float Weight = 1.f;

    /**
     * The intensity band this recording belongs to. A soft tap and a heavy slam are
     * different recordings, not one recording at two volumes — author light,
     * medium and heavy takes into bands like [0, 0.4], [0.3, 0.75], [0.65, 1].
     * Overlap the bands a little so the boundary doesn't sound like a switch.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinIntensity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxIntensity = 1.f;
};

/**
 * UPartySoundEvent
 *
 * The only audio thing gameplay code references. Gameplay says "this happened,
 * this hard, here" (UPartyAudioSubsystem::PlayEvent); the event decides which
 * recording plays and how it is shaped. Tweaking a sound is an asset edit, never
 * a code change — and the assets are themselves written by
 * AI/build_audio_assets.py from AI/audio_manifest.py.
 *
 * Every play is shaped by:
 *   - intensity (0..1): picks the recording band, then scales gain, pitch and
 *     low-pass cutoff (soft hits are quieter AND duller, which is what sells them);
 *   - jitter: a small random gain and pitch offset per play, so repeats never
 *     sound copy-pasted;
 *   - a per-source cooldown, so a body chattering on a floor doesn't machine-gun;
 *   - attenuation and concurrency assets, for distance and voice budget.
 *
 * Mix routing (SoundClass) is a property of the SoundWaves, set at import —
 * not here — so it also applies to anything that plays a wave directly.
 */
UCLASS(BlueprintType)
class PARTYAUDIO_API UPartySoundEvent : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variants", meta = (TitleProperty = "Sound"))
    TArray<FPartySoundVariant> Variants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variants")
    EPartySoundSelection Selection = EPartySoundSelection::ShuffleBag;

    /** Base gain for the whole event, in dB. The mix knob. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (UIMin = "-40.0", UIMax = "12.0"))
    float VolumeDb = 0.f;

    /** Each play is offset by a uniform random gain in +/- this many dB. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation", meta = (ClampMin = "0.0", UIMax = "6.0"))
    float VolumeJitterDb = 1.5f;

    /** Each play is offset by a uniform random pitch in +/- this many semitones. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation", meta = (ClampMin = "0.0", UIMax = "4.0"))
    float PitchJitterSemitones = 0.75f;

    /** Linear gain at intensity 0 (before VolumeDb). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = "0.0", UIMax = "2.0"))
    float VolumeAtMinIntensity = 0.25f;

    /** Linear gain at intensity 1 (before VolumeDb). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = "0.0", UIMax = "2.0"))
    float VolumeAtMaxIntensity = 1.f;

    /** Shape of the gain curve. 1 is linear; 2 keeps soft plays soft for longer. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = "0.1", UIMax = "4.0"))
    float VolumeCurveExponent = 1.f;

    /** Pitch offset at intensity 0, in semitones. Impacts usually go slightly UP here (small things hit lightly). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (UIMin = "-12.0", UIMax = "12.0"))
    float PitchAtMinIntensitySemitones = 0.f;

    /** Pitch offset at intensity 1, in semitones. Loops driven by speed usually go UP here. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (UIMin = "-12.0", UIMax = "12.0"))
    float PitchAtMaxIntensitySemitones = 0.f;

    /**
     * Low-pass cutoff at intensity 0, opening (log-scaled) to fully bright at 1.
     * 0 disables filtering. 1500-4000 Hz makes soft contacts read as soft.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intensity", meta = (ClampMin = "0.0", UIMax = "20000.0"))
    float LowPassAtMinIntensityHz = 0.f;

    /** A source cannot retrigger this event sooner than this. Per source (see PlayEvent's SourceKey). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback", meta = (ClampMin = "0.0", UIMax = "1.0"))
    float MinRetriggerSeconds = 0.05f;

    /** False plays it 2D (UI, stingers, ambience beds) regardless of the location passed in. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
    bool bSpatialized = true;

    /** Start loops at a random offset, so two instances of one bed never phase against each other. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
    bool bRandomStartTime = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
    TObjectPtr<USoundAttenuation> Attenuation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
    TObjectPtr<USoundConcurrency> Concurrency;

    /** The intensity-shaping parameters as the world-free maths wants them. */
    PartyAudio::FIntensityShaping GetShaping() const;

    /** Variants reduced to their selection-relevant data, index-parallel with Variants. */
    void GetBands(TArray<PartyAudio::FVariantBand>& OutBands) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
