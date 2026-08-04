#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PartySessionState.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Roster.*
//
// Roster-wiring guard for OctoOdyssey (slot 2 / L_GameC). See also
// PartyButtons.Session.Roster.GameModeOverridesUseReflectedClassNames
// (PartySessionStateTest.cpp), which checks the full-roster invariant this
// test's slot belongs to.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoRosterSlotTwoIsOctoOdyssey,
    "PartyButtons.Octo.Roster.SlotTwoIsOctoOdyssey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoRosterSlotTwoIsOctoOdyssey::RunTest(const FString& Parameters)
{
    FPartySessionState S;

    TestTrue(TEXT("Roster has a slot 2"), S.GameRoster.IsValidIndex(2));
    if (!S.GameRoster.IsValidIndex(2)) { return true; }

    const FPartyGameInfo& Slot2 = S.GameRoster[2];

    TestEqual(TEXT("Slot 2 maps to L_GameC"), Slot2.MapName, FName(TEXT("L_GameC")));
    TestEqual(TEXT("Slot 2 display name is Octo Odyssey"), Slot2.DisplayName, FString(TEXT("Octo Odyssey")));
    TestEqual(TEXT("Slot 2 GameMode override is AOctoGameMode's reflected path"),
        Slot2.GameModeClassPath, FString(TEXT("/Script/PartyButtons.OctoGameMode")));

    // Pitfall guard: the reflected class name must not contain the 'A' prefix
    // (see AI/design/architecture.md's travel-wiring pitfall).
    const int32 DotIndex = Slot2.GameModeClassPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    TestTrue(TEXT("Slot 2 override has a '.' separator"), DotIndex != INDEX_NONE);
    if (DotIndex != INDEX_NONE)
    {
        const FString ClassName = Slot2.GameModeClassPath.Mid(DotIndex + 1);
        TestFalse(TEXT("Slot 2 class name must not start with 'A'"), ClassName.StartsWith(TEXT("A")));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
