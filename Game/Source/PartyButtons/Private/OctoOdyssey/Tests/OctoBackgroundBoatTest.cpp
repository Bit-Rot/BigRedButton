#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OctoOdyssey/OctoBackgroundBoat.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Boat.*
//
// Tests for OctoBoat::WrapDistance, the world-free path math behind
// AOctoBackgroundBoat. The water sampling and pose need a world with an ocean
// and are checked by eye.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBoatWrapClosedLoop,
    "PartyButtons.Octo.Boat.WrapClosedLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBoatWrapClosedLoop::RunTest(const FString& Parameters)
{
    bool bBackward = true;
    TestEqual(TEXT("Inside the loop is unchanged"), OctoBoat::WrapDistance(250.f, 1000.f, true, bBackward), 250.f, 0.01f);
    TestFalse(TEXT("A loop is never backward"), bBackward);
    TestEqual(TEXT("Past the end wraps"), OctoBoat::WrapDistance(2250.f, 1000.f, true, bBackward), 250.f, 0.01f);
    TestEqual(TEXT("Negative (reverse) wraps from the end"), OctoBoat::WrapDistance(-250.f, 1000.f, true, bBackward), 750.f, 0.01f);
    TestEqual(TEXT("Zero-length spline is safe"), OctoBoat::WrapDistance(123.f, 0.f, true, bBackward), 0.f, 0.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBoatWrapOpenPingPongs,
    "PartyButtons.Octo.Boat.WrapOpenPingPongs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBoatWrapOpenPingPongs::RunTest(const FString& Parameters)
{
    bool bBackward = true;
    TestEqual(TEXT("Outbound leg"), OctoBoat::WrapDistance(300.f, 1000.f, false, bBackward), 300.f, 0.01f);
    TestFalse(TEXT("Outbound is forward"), bBackward);

    TestEqual(TEXT("Return leg comes back"), OctoBoat::WrapDistance(1300.f, 1000.f, false, bBackward), 700.f, 0.01f);
    TestTrue(TEXT("Return leg is backward"), bBackward);

    TestEqual(TEXT("Full period is back at the start"), OctoBoat::WrapDistance(2300.f, 1000.f, false, bBackward), 300.f, 0.01f);
    TestFalse(TEXT("Second outbound is forward"), bBackward);

    TestEqual(TEXT("Negative distance lands on the return leg"), OctoBoat::WrapDistance(-300.f, 1000.f, false, bBackward), 300.f, 0.01f);
    TestTrue(TEXT("Negative distance is backward"), bBackward);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
