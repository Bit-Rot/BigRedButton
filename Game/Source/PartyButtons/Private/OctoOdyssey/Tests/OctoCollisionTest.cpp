#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/CollisionProfile.h"
#include "OctoOdyssey/OctoTypes.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Collision.*
//
// The trigger volumes' collision profile lives in Config/DefaultEngine.ini, so
// nothing in C++ stops it being edited away in Project Settings. These tests
// pin the two properties the game actually depends on, world-free:
//
//   1. Its object type is NOT one AOctoPawn's arm/head sweeps ask for. Those
//      are SweepSingleByObjectType queries, which report every object-type
//      match as a BLOCKING hit no matter what the responses say -- a volume
//      that shares an object type with world geometry is an invisible wall for
//      an extending arm to plant on and push off. This is the regression.
//
//   2. It still overlaps PhysicsBody, or nothing ever fires: AOctoPawn's
//      BodySphere is a PhysicsActor.
// --------------------------------------------------------------------------

namespace
{
    bool GetTriggerProfile(FCollisionResponseTemplate& OutTemplate)
    {
        const UCollisionProfile* Profiles = UCollisionProfile::Get();
        return Profiles && Profiles->GetProfileTemplate(OctoCollision::TriggerProfile, OutTemplate);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoCollisionTriggerProfileExists,
    "PartyButtons.Octo.Collision.TriggerProfileExists",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoCollisionTriggerProfileExists::RunTest(const FString& Parameters)
{
    FCollisionResponseTemplate Template;

    if (!TestTrue(FString::Printf(TEXT("The '%s' collision profile is defined (see DefaultEngine.ini)"),
            OctoCollision::TriggerProfile), GetTriggerProfile(Template)))
    {
        return false;
    }

    TestEqual(TEXT("Trigger volumes are query-only -- they must never push the octopus physically"),
        static_cast<int32>(Template.CollisionEnabled.GetValue()),
        static_cast<int32>(ECollisionEnabled::QueryOnly));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoCollisionTriggerProfileIsInvisibleToArmSweeps,
    "PartyButtons.Octo.Collision.TriggerProfileIsInvisibleToArmSweeps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoCollisionTriggerProfileIsInvisibleToArmSweeps::RunTest(const FString& Parameters)
{
    FCollisionResponseTemplate Template;

    if (!TestTrue(TEXT("The trigger profile is defined"), GetTriggerProfile(Template)))
    {
        return false;
    }

    // Every object type AOctoPawn::TickArm and ResolveHeadCollision can query,
    // bSweepPhysicsBodies included. Sharing any of them makes the volume solid.
    const ECollisionChannel SweptTypes[] = { ECC_WorldStatic, ECC_WorldDynamic, ECC_PhysicsBody };

    for (const ECollisionChannel Swept : SweptTypes)
    {
        TestFalse(FString::Printf(
                TEXT("Trigger volumes must not use object type %d -- the arm sweep would treat them as solid"),
                static_cast<int32>(Swept)),
            Template.ObjectType == Swept);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoCollisionTriggerProfileStillOverlapsTheOctopus,
    "PartyButtons.Octo.Collision.TriggerProfileStillOverlapsTheOctopus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoCollisionTriggerProfileStillOverlapsTheOctopus::RunTest(const FString& Parameters)
{
    FCollisionResponseTemplate Template;

    if (!TestTrue(TEXT("The trigger profile is defined"), GetTriggerProfile(Template)))
    {
        return false;
    }

    // AOctoPawn's BodySphere and arm colliders are all PhysicsActor.
    TestEqual(TEXT("Trigger volumes overlap PhysicsBody, so the octopus still arms them"),
        static_cast<int32>(Template.ResponseToChannels.GetResponse(ECC_PhysicsBody)),
        static_cast<int32>(ECR_Overlap));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
