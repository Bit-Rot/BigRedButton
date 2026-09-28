#include "PartyAudioMath.h"

namespace PartyAudio
{
    void FilterByIntensity(TConstArrayView<FVariantBand> Bands, float Intensity, TArray<int32>& OutCandidates)
    {
        OutCandidates.Reset();

        float NearestGap = TNumericLimits<float>::Max();
        for (int32 i = 0; i < Bands.Num(); i++)
        {
            const FVariantBand& Band = Bands[i];
            if (Band.Weight <= 0.f)
            {
                continue;
            }

            const float Gap = Intensity < Band.MinIntensity ? Band.MinIntensity - Intensity
                            : Intensity > Band.MaxIntensity ? Intensity - Band.MaxIntensity
                            : 0.f;
            NearestGap = FMath::Min(NearestGap, Gap);
        }

        // Second pass keeps every band tied for nearest — 0 in the normal case,
        // i.e. every band that actually contains Intensity.
        for (int32 i = 0; i < Bands.Num(); i++)
        {
            const FVariantBand& Band = Bands[i];
            if (Band.Weight <= 0.f)
            {
                continue;
            }

            const float Gap = Intensity < Band.MinIntensity ? Band.MinIntensity - Intensity
                            : Intensity > Band.MaxIntensity ? Intensity - Band.MaxIntensity
                            : 0.f;
            if (FMath::IsNearlyEqual(Gap, NearestGap, 1.e-4f))
            {
                OutCandidates.Add(i);
            }
        }
    }

    int32 PickWeighted(TConstArrayView<FVariantBand> Bands, TConstArrayView<int32> Candidates, float Roll01)
    {
        if (Candidates.Num() == 0)
        {
            return INDEX_NONE;
        }

        float Total = 0.f;
        for (int32 Index : Candidates)
        {
            Total += FMath::Max(0.f, Bands[Index].Weight);
        }
        if (Total <= 0.f)
        {
            return Candidates[0];
        }

        float Target = FMath::Clamp(Roll01, 0.f, 1.f) * Total;
        for (int32 Index : Candidates)
        {
            Target -= FMath::Max(0.f, Bands[Index].Weight);
            if (Target < 0.f)
            {
                return Index;
            }
        }
        return Candidates.Last(); // Roll01 == 1 (or float slop) lands on the last one.
    }

    int32 PickNoRepeat(TConstArrayView<FVariantBand> Bands, TConstArrayView<int32> Candidates, int32 Previous, FRandomStream& Stream)
    {
        if (Candidates.Num() > 1 && Candidates.Contains(Previous))
        {
            TArray<int32, TInlineAllocator<16>> Others;
            for (int32 Index : Candidates)
            {
                if (Index != Previous)
                {
                    Others.Add(Index);
                }
            }
            return PickWeighted(Bands, Others, Stream.GetFraction());
        }
        return PickWeighted(Bands, Candidates, Stream.GetFraction());
    }

    int32 FShuffleBag::Draw(TConstArrayView<int32> Candidates, FRandomStream& Stream)
    {
        if (Candidates.Num() == 0)
        {
            return INDEX_NONE;
        }

        Remaining.RemoveAll([&Candidates](int32 Index) { return !Candidates.Contains(Index); });

        if (Remaining.Num() == 0)
        {
            Remaining.Append(Candidates.GetData(), Candidates.Num());
            // Fisher-Yates.
            for (int32 i = Remaining.Num() - 1; i > 0; i--)
            {
                Remaining.Swap(i, Stream.RandRange(0, i));
            }
            // Draws pop from the back; keep the refill from opening with a repeat.
            if (Remaining.Num() > 1 && Remaining.Last() == Last)
            {
                Remaining.Swap(Remaining.Num() - 1, 0);
            }
        }

        Last = Remaining.Pop(EAllowShrinking::No);
        return Last;
    }

    bool FCooldownGate::TryPass(double Now, float MinInterval)
    {
        // A microsecond of slack: time deltas carry float error (10.1 - 10.0 is a
        // hair under 0.1), and a retrigger landing exactly on the interval should pass.
        if (Now - LastPassTime < static_cast<double>(MinInterval) - 1.0e-6)
        {
            return false;
        }
        LastPassTime = Now;
        return true;
    }

    float FIntensityShaping::EvaluateVolume(float Intensity) const
    {
        const float T = FMath::Pow(FMath::Clamp(Intensity, 0.f, 1.f), FMath::Max(0.01f, VolumeExponent));
        return FMath::Max(0.f, FMath::Lerp(VolumeAtMin, VolumeAtMax, T));
    }

    float FIntensityShaping::EvaluatePitchSemitones(float Intensity) const
    {
        return FMath::Lerp(PitchAtMinSemitones, PitchAtMaxSemitones, FMath::Clamp(Intensity, 0.f, 1.f));
    }

    float FIntensityShaping::EvaluateLowPassHz(float Intensity) const
    {
        if (LowPassAtMinHz <= 0.f)
        {
            return 0.f;
        }
        // Interpolate in log-frequency: equal steps of intensity are equal musical
        // steps of brightness, which is how the ear hears a filter sweep.
        const float LogMin = FMath::Loge(FMath::Min(LowPassAtMinHz, FullyOpenLowPassHz));
        const float LogMax = FMath::Loge(FullyOpenLowPassHz);
        return FMath::Exp(FMath::Lerp(LogMin, LogMax, FMath::Clamp(Intensity, 0.f, 1.f)));
    }

    float DbToLinear(float Db)
    {
        return FMath::Pow(10.f, Db / 20.f);
    }

    float SemitonesToPitch(float Semitones)
    {
        return FMath::Pow(2.f, Semitones / 12.f);
    }

    float NormalizeIntensity(float Value, float Floor, float Ceiling)
    {
        if (Value < Floor)
        {
            return 0.f;
        }
        if (Ceiling <= Floor)
        {
            return 1.f;
        }
        return FMath::Clamp((Value - Floor) / (Ceiling - Floor), 0.f, 1.f);
    }

    float SmoothToward(float Current, float Target, float DeltaSeconds, float AttackSeconds, float ReleaseSeconds)
    {
        const float Tau = Target > Current ? AttackSeconds : ReleaseSeconds;
        if (Tau <= 0.f || DeltaSeconds <= 0.f)
        {
            return Tau <= 0.f ? Target : Current;
        }
        const float Alpha = 1.f - FMath::Exp(-DeltaSeconds / Tau);
        return Current + (Target - Current) * Alpha;
    }
}
