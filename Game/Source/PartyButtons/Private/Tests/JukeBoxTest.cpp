#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "JukeBoxVolumeEase.h"

// --------------------------------------------------------------------------
// PartyButtons.Audio.JukeBox.*
//
// Tests for FJukeBoxVolumeEase, the world-free timing half of AJukeBox. No
// audio device, no actor — the jukebox itself is a thin shell that pushes
// GetCurrent() into an audio component each tick.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FJukeBoxEaseReachesTargetExactly,
    "PartyButtons.Audio.JukeBox.EaseReachesTargetExactly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJukeBoxEaseReachesTargetExactly::RunTest(const FString& Parameters)
{
    FJukeBoxVolumeEase Ease(0.15f);
    Ease.EaseTo(0.8f, 1.f);

    TestTrue(TEXT("Easing after EaseTo"), Ease.IsEasing());

    // Uneven frame times that overshoot the duration.
    for (int32 i = 0; i < 7; i++)
    {
        Ease.Advance(0.17f);
    }

    TestFalse(TEXT("Done after the duration"), Ease.IsEasing());
    TestEqual(TEXT("Lands exactly on the target"), Ease.GetCurrent(), 0.8f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FJukeBoxEaseIsMonotonicAndSmooth,
    "PartyButtons.Audio.JukeBox.EaseIsMonotonicAndSmooth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJukeBoxEaseIsMonotonicAndSmooth::RunTest(const FString& Parameters)
{
    FJukeBoxVolumeEase Ease(0.f);
    Ease.EaseTo(1.f, 1.f);

    float Previous = Ease.GetCurrent();
    float FirstStep = 0.f;
    float MidStep   = 0.f;

    for (int32 i = 1; i <= 10; i++)
    {
        const float Now = Ease.Advance(0.1f);
        TestTrue(FString::Printf(TEXT("Never goes backwards (step %d)"), i), Now >= Previous);

        if (i == 1) { FirstStep = Now - Previous; }
        if (i == 5) { MidStep   = Now - Previous; }
        Previous = Now;
    }

    // Ease-in: the first tenth moves much less than the middle tenth.
    TestTrue(TEXT("Starts gently (smoothstep, not linear)"), FirstStep < MidStep * 0.5f);

    FJukeBoxVolumeEase Half(0.f);
    Half.EaseTo(1.f, 1.f);
    TestTrue(TEXT("Halfway through is halfway there"), FMath::IsNearlyEqual(Half.Advance(0.5f), 0.5f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FJukeBoxEaseRetargetDoesNotJump,
    "PartyButtons.Audio.JukeBox.RetargetMidEaseDoesNotJump",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJukeBoxEaseRetargetDoesNotJump::RunTest(const FString& Parameters)
{
    // Menu -> play -> menu before the first ease has finished.
    FJukeBoxVolumeEase Ease(0.15f);
    Ease.EaseTo(0.8f, 1.f);
    const float MidWay = Ease.Advance(0.4f);

    Ease.EaseTo(0.15f, 1.f);
    TestEqual(TEXT("Retarget starts from where it was"), Ease.GetCurrent(), MidWay);

    const float Next = Ease.Advance(1.f / 60.f);
    TestTrue(TEXT("First frame after retarget moves only a little"), FMath::Abs(Next - MidWay) < 0.01f);

    Ease.Advance(2.f);
    TestEqual(TEXT("And settles on the new target"), Ease.GetCurrent(), 0.15f);

    // Round trips land on the same two values every time — nothing accumulates.
    for (int32 Trip = 0; Trip < 5; Trip++)
    {
        Ease.EaseTo(0.8f, 1.f);
        Ease.Advance(0.3f);
        Ease.EaseTo(0.15f, 1.f);
        Ease.Advance(0.2f);
    }
    Ease.EaseTo(0.8f, 1.f);
    Ease.Advance(5.f);
    TestEqual(TEXT("Back and forth still lands on the play volume"), Ease.GetCurrent(), 0.8f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FJukeBoxEaseZeroDurationAndClamp,
    "PartyButtons.Audio.JukeBox.ZeroDurationSnapsAndNegativeClamps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FJukeBoxEaseZeroDurationAndClamp::RunTest(const FString& Parameters)
{
    FJukeBoxVolumeEase Ease(1.f);

    Ease.EaseTo(0.3f, 0.f);
    TestFalse(TEXT("Zero duration is not an ease"), Ease.IsEasing());
    TestEqual(TEXT("Zero duration snaps"), Ease.GetCurrent(), 0.3f);

    Ease.SetImmediate(-2.f);
    TestEqual(TEXT("Negative volume clamps to silence"), Ease.GetCurrent(), 0.f);

    Ease.EaseTo(-1.f, 1.f);
    Ease.Advance(1.f);
    TestEqual(TEXT("Negative target clamps to silence"), Ease.GetCurrent(), 0.f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
