#include "PartySoundEvent.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

PartyAudio::FIntensityShaping UPartySoundEvent::GetShaping() const
{
    PartyAudio::FIntensityShaping Shaping;
    Shaping.VolumeAtMin         = VolumeAtMinIntensity;
    Shaping.VolumeAtMax         = VolumeAtMaxIntensity;
    Shaping.VolumeExponent      = VolumeCurveExponent;
    Shaping.PitchAtMinSemitones = PitchAtMinIntensitySemitones;
    Shaping.PitchAtMaxSemitones = PitchAtMaxIntensitySemitones;
    Shaping.LowPassAtMinHz      = LowPassAtMinIntensityHz;
    return Shaping;
}

void UPartySoundEvent::GetBands(TArray<PartyAudio::FVariantBand>& OutBands) const
{
    OutBands.Reset(Variants.Num());
    for (const FPartySoundVariant& Variant : Variants)
    {
        PartyAudio::FVariantBand& Band = OutBands.AddDefaulted_GetRef();
        // A variant with no sound can never play; treat it as disabled rather
        // than letting the picker choose silence.
        Band.Weight       = Variant.Sound ? Variant.Weight : 0.f;
        Band.MinIntensity = Variant.MinIntensity;
        Band.MaxIntensity = Variant.MaxIntensity;
    }
}

#if WITH_EDITOR
EDataValidationResult UPartySoundEvent::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    bool bAnyPlayable = false;
    for (int32 i = 0; i < Variants.Num(); i++)
    {
        const FPartySoundVariant& Variant = Variants[i];
        bAnyPlayable |= Variant.Sound && Variant.Weight > 0.f;
        if (Variant.MinIntensity > Variant.MaxIntensity)
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Variant %d has MinIntensity > MaxIntensity."), i)));
            Result = EDataValidationResult::Invalid;
        }
    }
    if (!bAnyPlayable)
    {
        Context.AddError(FText::FromString(TEXT("No variant has both a sound and a weight above zero.")));
        Result = EDataValidationResult::Invalid;
    }
    return Result;
}
#endif
