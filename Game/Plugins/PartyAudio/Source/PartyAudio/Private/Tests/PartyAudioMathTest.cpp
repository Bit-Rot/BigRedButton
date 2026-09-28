#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PartyAudioMath.h"

#define PartyAudioTestFlags (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// --------------------------------------------------------------------------
// PartyButtons.Audio.Framework.*
//
// Tests for the PartyAudio:: maths — variant selection, shaping, gating and
// smoothing. No audio device, no world, no assets; the subsystem and components
// are thin shells over these.
// --------------------------------------------------------------------------

namespace
{
    TArray<PartyAudio::FVariantBand> ThreeBands()
    {
        // Light, medium, heavy with small overlaps — how impacts are authored.
        return {
            { 1.f, 0.00f, 0.40f },
            { 1.f, 0.30f, 0.75f },
            { 1.f, 0.65f, 1.00f },
        };
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioFilterPicksContainingBands,
    "PartyButtons.Audio.Framework.FilterPicksContainingBands", PartyAudioTestFlags)

bool FPartyAudioFilterPicksContainingBands::RunTest(const FString& Parameters)
{
    const TArray<PartyAudio::FVariantBand> Bands = ThreeBands();
    TArray<int32> Out;

    PartyAudio::FilterByIntensity(Bands, 0.1f, Out);
    TestTrue(TEXT("Soft hit: light only"), Out == TArray<int32>{ 0 });

    PartyAudio::FilterByIntensity(Bands, 0.35f, Out);
    TestTrue(TEXT("Overlap: light and medium"), Out == TArray<int32>{ 0, 1 });

    PartyAudio::FilterByIntensity(Bands, 1.f, Out);
    TestTrue(TEXT("Hardest hit: heavy only"), Out == TArray<int32>{ 2 });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioFilterFallsBackToNearest,
    "PartyButtons.Audio.Framework.FilterFallsBackToNearestAndSkipsDisabled", PartyAudioTestFlags)

bool FPartyAudioFilterFallsBackToNearest::RunTest(const FString& Parameters)
{
    // A gap in the authoring between 0.4 and 0.6, and a disabled variant covering it.
    const TArray<PartyAudio::FVariantBand> Bands = {
        { 1.f, 0.0f, 0.4f },
        { 0.f, 0.0f, 1.0f },
        { 1.f, 0.6f, 1.0f },
    };
    TArray<int32> Out;

    PartyAudio::FilterByIntensity(Bands, 0.45f, Out);
    TestTrue(TEXT("Nearest band wins inside a gap; weight 0 never does"), Out == TArray<int32>{ 0 });

    PartyAudio::FilterByIntensity(Bands, 0.5f, Out);
    TestTrue(TEXT("Exactly between two bands keeps both"), Out == TArray<int32>{ 0, 2 });

    PartyAudio::FilterByIntensity(TArray<PartyAudio::FVariantBand>{ { 0.f, 0.f, 1.f } }, 0.5f, Out);
    TestEqual(TEXT("Nothing playable -> no candidates"), Out.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioWeightedPickFollowsWeights,
    "PartyButtons.Audio.Framework.WeightedPickFollowsWeights", PartyAudioTestFlags)

bool FPartyAudioWeightedPickFollowsWeights::RunTest(const FString& Parameters)
{
    const TArray<PartyAudio::FVariantBand> Bands = { { 1.f, 0.f, 1.f }, { 3.f, 0.f, 1.f } };
    const TArray<int32> All = { 0, 1 };

    TestEqual(TEXT("Roll 0 -> first"), PartyAudio::PickWeighted(Bands, All, 0.f), 0);
    TestEqual(TEXT("Roll just under 1/4 -> first"), PartyAudio::PickWeighted(Bands, All, 0.24f), 0);
    TestEqual(TEXT("Roll just over 1/4 -> second"), PartyAudio::PickWeighted(Bands, All, 0.26f), 1);
    TestEqual(TEXT("Roll 1 -> last"), PartyAudio::PickWeighted(Bands, All, 1.f), 1);
    TestEqual(TEXT("No candidates -> none"), PartyAudio::PickWeighted(Bands, TArray<int32>{}, 0.5f), INDEX_NONE);

    FRandomStream Stream(1234);
    int32 Counts[2] = { 0, 0 };
    for (int32 i = 0; i < 4000; i++)
    {
        Counts[PartyAudio::PickWeighted(Bands, All, Stream.GetFraction())]++;
    }
    const float Ratio = static_cast<float>(Counts[1]) / static_cast<float>(Counts[0]);
    TestTrue(FString::Printf(TEXT("3:1 weights give ~3:1 picks (got %.2f)"), Ratio), Ratio > 2.6f && Ratio < 3.4f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioNoRepeatNeverRepeats,
    "PartyButtons.Audio.Framework.NoRepeatNeverRepeats", PartyAudioTestFlags)

bool FPartyAudioNoRepeatNeverRepeats::RunTest(const FString& Parameters)
{
    const TArray<PartyAudio::FVariantBand> Bands = { { 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f } };
    const TArray<int32> All = { 0, 1, 2 };
    FRandomStream Stream(99);

    int32 Previous = INDEX_NONE;
    for (int32 i = 0; i < 500; i++)
    {
        const int32 Pick = PartyAudio::PickNoRepeat(Bands, All, Previous, Stream);
        if (Pick == Previous)
        {
            AddError(FString::Printf(TEXT("Repeated %d at draw %d"), Pick, i));
            return false;
        }
        Previous = Pick;
    }

    TestEqual(TEXT("A single candidate still plays"), PartyAudio::PickNoRepeat(Bands, TArray<int32>{ 1 }, 1, Stream), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioShuffleBagCoversAndNeverRepeats,
    "PartyButtons.Audio.Framework.ShuffleBagCoversEveryVariantAndNeverRepeats", PartyAudioTestFlags)

bool FPartyAudioShuffleBagCoversAndNeverRepeats::RunTest(const FString& Parameters)
{
    const TArray<int32> All = { 0, 1, 2, 3, 4 };
    PartyAudio::FShuffleBag Bag;
    FRandomStream Stream(7);

    int32 Previous = INDEX_NONE;
    for (int32 Round = 0; Round < 200; Round++)
    {
        TSet<int32> Seen;
        for (int32 i = 0; i < All.Num(); i++)
        {
            const int32 Pick = Bag.Draw(All, Stream);
            if (Pick == Previous)
            {
                AddError(FString::Printf(TEXT("Back-to-back repeat of %d in round %d"), Pick, Round));
                return false;
            }
            Seen.Add(Pick);
            Previous = Pick;
        }
        if (Seen.Num() != All.Num())
        {
            AddError(FString::Printf(TEXT("Round %d played %d distinct variants, expected %d"), Round, Seen.Num(), All.Num()));
            return false;
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioShuffleBagFollowsCandidateChanges,
    "PartyButtons.Audio.Framework.ShuffleBagFollowsCandidateChanges", PartyAudioTestFlags)

bool FPartyAudioShuffleBagFollowsCandidateChanges::RunTest(const FString& Parameters)
{
    PartyAudio::FShuffleBag Bag;
    FRandomStream Stream(3);

    Bag.Draw(TArray<int32>{ 0, 1, 2 }, Stream); // Light band, bag now holds two of these.

    // A hard hit: only the heavy variants are allowed, whatever the bag holds.
    for (int32 i = 0; i < 20; i++)
    {
        const int32 Pick = Bag.Draw(TArray<int32>{ 5, 6 }, Stream);
        TestTrue(TEXT("Only current candidates are drawn"), Pick == 5 || Pick == 6);
    }

    TestEqual(TEXT("Empty candidates -> none"), Bag.Draw(TArray<int32>{}, Stream), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioCooldownGate,
    "PartyButtons.Audio.Framework.CooldownGate", PartyAudioTestFlags)

bool FPartyAudioCooldownGate::RunTest(const FString& Parameters)
{
    PartyAudio::FCooldownGate Gate;
    TestTrue(TEXT("First play passes"), Gate.TryPass(10.0, 0.1f));
    TestFalse(TEXT("Inside the interval is blocked"), Gate.TryPass(10.05, 0.1f));
    TestTrue(TEXT("After the interval passes"), Gate.TryPass(10.1, 0.1f));
    TestFalse(TEXT("A blocked attempt doesn't re-arm the gate"), Gate.TryPass(10.15, 0.1f));
    TestTrue(TEXT("Measured from the last PASS"), Gate.TryPass(10.2, 0.1f));
    TestTrue(TEXT("Zero interval never blocks"), Gate.TryPass(10.2, 0.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioNormalizeIntensity,
    "PartyButtons.Audio.Framework.NormalizeIntensityClamps", PartyAudioTestFlags)

bool FPartyAudioNormalizeIntensity::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Below floor is silent"), PartyAudio::NormalizeIntensity(50.f, 60.f, 1200.f), 0.f);
    TestEqual(TEXT("At floor is 0"), PartyAudio::NormalizeIntensity(60.f, 60.f, 1200.f), 0.f);
    TestEqual(TEXT("Midpoint"), PartyAudio::NormalizeIntensity(630.f, 60.f, 1200.f), 0.5f);
    TestEqual(TEXT("Above ceiling clamps to 1"), PartyAudio::NormalizeIntensity(5000.f, 60.f, 1200.f), 1.f);
    TestEqual(TEXT("Degenerate range steps at floor"), PartyAudio::NormalizeIntensity(100.f, 100.f, 100.f), 1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioShaping,
    "PartyButtons.Audio.Framework.IntensityShaping", PartyAudioTestFlags)

bool FPartyAudioShaping::RunTest(const FString& Parameters)
{
    PartyAudio::FIntensityShaping Shaping;
    Shaping.VolumeAtMin = 0.2f;
    Shaping.VolumeAtMax = 1.f;
    Shaping.VolumeExponent = 2.f;
    Shaping.PitchAtMinSemitones = 1.f;
    Shaping.PitchAtMaxSemitones = -2.f;
    Shaping.LowPassAtMinHz = 2000.f;

    TestEqual(TEXT("Volume at 0"), Shaping.EvaluateVolume(0.f), 0.2f);
    TestEqual(TEXT("Volume at 1"), Shaping.EvaluateVolume(1.f), 1.f);
    TestEqual(TEXT("Exponent 2 keeps the middle soft"), Shaping.EvaluateVolume(0.5f), 0.4f, 1e-4f);
    TestEqual(TEXT("Pitch interpolates"), Shaping.EvaluatePitchSemitones(0.5f), -0.5f);
    TestEqual(TEXT("Low-pass at 0"), Shaping.EvaluateLowPassHz(0.f), 2000.f, 0.5f);
    TestEqual(TEXT("Low-pass fully open at 1"), Shaping.EvaluateLowPassHz(1.f), PartyAudio::FullyOpenLowPassHz, 1.f);
    TestEqual(TEXT("Low-pass is log-interpolated (geometric mean at 0.5)"),
              Shaping.EvaluateLowPassHz(0.5f), FMath::Sqrt(2000.f * PartyAudio::FullyOpenLowPassHz), 1.f);

    Shaping.LowPassAtMinHz = 0.f;
    TestEqual(TEXT("0 Hz disables the filter"), Shaping.EvaluateLowPassHz(0.3f), 0.f);

    TestEqual(TEXT("-6 dB is about half"), PartyAudio::DbToLinear(-6.f), 0.501f, 1e-3f);
    TestEqual(TEXT("+12 semitones is an octave"), PartyAudio::SemitonesToPitch(12.f), 2.f, 1e-4f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyAudioSmoothToward,
    "PartyButtons.Audio.Framework.SmoothTowardAttackAndRelease", PartyAudioTestFlags)

bool FPartyAudioSmoothToward::RunTest(const FString& Parameters)
{
    // One time constant covers ~63% of the gap, independent of frame rate.
    float Up = 0.f;
    for (int32 i = 0; i < 10; i++)
    {
        Up = PartyAudio::SmoothToward(Up, 1.f, 0.01f, 0.1f, 1.f);
    }
    TestEqual(TEXT("Attack: 63% after one tau"), Up, 0.632f, 0.01f);

    float UpCoarse = PartyAudio::SmoothToward(0.f, 1.f, 0.1f, 0.1f, 1.f);
    TestEqual(TEXT("Same result at a coarser step"), UpCoarse, Up, 0.01f);

    float Down = PartyAudio::SmoothToward(1.f, 0.f, 0.1f, 0.1f, 1.f);
    TestTrue(TEXT("Release uses the slower constant"), Down > 0.85f);

    TestEqual(TEXT("Zero constant snaps"), PartyAudio::SmoothToward(0.f, 1.f, 0.016f, 0.f, 0.f), 1.f);
    TestEqual(TEXT("Zero dt holds"), PartyAudio::SmoothToward(0.3f, 1.f, 0.f, 0.1f, 0.1f), 0.3f);
    return true;
}

#undef PartyAudioTestFlags

#endif // WITH_DEV_AUTOMATION_TESTS
