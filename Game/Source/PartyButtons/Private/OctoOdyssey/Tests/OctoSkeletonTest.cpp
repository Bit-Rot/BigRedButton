#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "OctoOdyssey/OctoSkeleton.h"
#include "OctoOdyssey/OctoTuning.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Skeleton.*
//
// The guard rail on /Game/OctoOdyssey/SK_Okto. AOctoPawn poses that mesh from
// numbers measured out of its REFERENCE skeleton (OctoSkeleton::BuildArmBones),
// so a re-export that changed bone naming, orientation, axis convention or
// modelled reach would silently mis-pose the octopus instead of failing. These
// tests turn that into a red test.
//
// They assert the mesh against FOctoTuning's DEFAULTS, because those defaults
// are what the mesh was modelled from — the two are one artifact in two files.
// --------------------------------------------------------------------------

namespace
{
    const TCHAR* const OctoMeshPath = TEXT("/Game/OctoOdyssey/SK_Okto.SK_Okto");

    /** cm of slack. The mesh comes from Blender via FBX, so exact equality is not on offer. */
    constexpr float Tolerance = 0.1f;

    USkeletalMesh* LoadOctoMesh()
    {
        return LoadObject<USkeletalMesh>(nullptr, OctoMeshPath);
    }

    /** The extension an arm can actually reach, which is what the mesh is modelled at. */
    float EffectiveMaxExtension(const FOctoTuning& Tuning)
    {
        return FMath::Min(
            Tuning.ArmMaxExtension,
            OctoArm::MaxSafeExtension(Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight));
    }
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoSkeletonRigResolves,
    "PartyButtons.Octo.Skeleton.RigResolves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoSkeletonRigResolves::RunTest(const FString& Parameters)
{
    USkeletalMesh* Mesh = LoadOctoMesh();
    if (!TestNotNull(TEXT("SK_Okto loads"), Mesh))
    {
        return false;
    }

    TArray<FOctoArmBones> Arms;
    if (!TestTrue(TEXT("Every arm's bones resolve"), OctoSkeleton::BuildArmBones(*Mesh, Arms)))
    {
        return false;
    }

    TestEqual(TEXT("One entry per arm"), Arms.Num(), OctoArm::NumArms);

    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();

    for (int32 i = 0; i < Arms.Num(); i++)
    {
        const FOctoArmBones& Arm = Arms[i];

        TestEqual(FString::Printf(TEXT("Arm %d resolves to bone Arm%d"), i, i + 1),
            RefSkeleton.GetBoneName(Arm.ChainRootIndex),
            FName(*FString::Printf(TEXT("Arm%d"), i + 1)));
        TestEqual(FString::Printf(TEXT("Arm %d resolves to bone Hand%d"), i, i + 1),
            RefSkeleton.GetBoneName(Arm.HandIndex),
            FName(*FString::Printf(TEXT("Hand%d"), i + 1)));

        TestTrue(FString::Printf(TEXT("Arm %d's rest direction is unit length"), i),
            FMath::IsNearlyEqual(Arm.RestDirection.Size(), 1.f, 0.001f));
        TestTrue(FString::Printf(TEXT("Arm %d's length axis is unit length"), i),
            FMath::IsNearlyEqual(Arm.LengthAxis.Size(), 1.f, 0.001f));
        TestTrue(FString::Printf(TEXT("Arm %d has a positive chain length"), i),
            Arm.RestChainLength > 0.f);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoSkeletonArmsPointWhereTheGameExpects,
    "PartyButtons.Octo.Skeleton.ArmsPointWhereTheGameExpects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoSkeletonArmsPointWhereTheGameExpects::RunTest(const FString& Parameters)
{
    USkeletalMesh* Mesh = LoadOctoMesh();
    if (!TestNotNull(TEXT("SK_Okto loads"), Mesh))
    {
        return false;
    }

    TArray<FOctoArmBones> Arms;
    if (!TestTrue(TEXT("Every arm's bones resolve"), OctoSkeleton::BuildArmBones(*Mesh, Arms)))
    {
        return false;
    }

    // The asset is authored at a ZERO arm-angle offset; AOctoPawn::RebuildGeometry
    // supplies Tuning.ArmAngleOffsetDegrees as a roll on the mesh component. If that
    // ever stops being true, the arms and the colliders separate by that angle.
    for (int32 i = 0; i < Arms.Num(); i++)
    {
        const FVector Expected = OctoArm::ArmDirectionLocal(i, OctoArm::NumArms, 0.f);
        const FVector Actual   = Arms[i].RestDirection;

        TestTrue(
            FString::Printf(TEXT("Arm %d points along ArmDirectionLocal(%d, 8, 0) — expected %s, got %s"),
                i, i, *Expected.ToString(), *Actual.ToString()),
            Actual.Equals(Expected, 0.001f));

        // The octopus's DOF lock pins it to the Y-Z plane, so an arm with any X
        // component would swing the mesh out of the play plane.
        TestTrue(FString::Printf(TEXT("Arm %d's rest direction is planar (X ~ 0)"), i),
            FMath::IsNearlyZero(Actual.X, 0.001f));
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoSkeletonReachMatchesTuning,
    "PartyButtons.Octo.Skeleton.ReachMatchesTuning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoSkeletonReachMatchesTuning::RunTest(const FString& Parameters)
{
    USkeletalMesh* Mesh = LoadOctoMesh();
    if (!TestNotNull(TEXT("SK_Okto loads"), Mesh))
    {
        return false;
    }

    TArray<FOctoArmBones> Arms;
    if (!TestTrue(TEXT("Every arm's bones resolve"), OctoSkeleton::BuildArmBones(*Mesh, Arms)))
    {
        return false;
    }

    const FOctoTuning Defaults;
    const float SafeMax = EffectiveMaxExtension(Defaults);

    // The whole premise of the pose driver: SK_Okto's reference pose IS the octopus
    // at full extension, so the modelled tip lands exactly on the collider's outer cap.
    // Note this is the EFFECTIVE max — the mesh was modelled against what the arm can
    // reach, not against what the tunable says. At the shipping defaults the two coincide
    // at 70; the Min still matters because raising ArmMaxExtension alone cannot move the
    // reach past MaxSafeExtension, which is what TickArm clamps to.
    const float ExpectedTip = OctoArm::HandCenterOffset(
        Defaults.SphereRadius, Defaults.ArmRadius, Defaults.ArmHalfHeight, SafeMax);

    for (int32 i = 0; i < Arms.Num(); i++)
    {
        const FOctoArmBones& Arm = Arms[i];
        const float ModelledTip = Arm.ChainOriginOffset + Arm.RestChainLength;

        TestTrue(
            FString::Printf(TEXT("Arm %d's modelled reach is %.3f, expected %.3f (SphereRadius %.1f + max extension %.1f)"),
                i, ModelledTip, ExpectedTip, Defaults.SphereRadius, SafeMax),
            FMath::IsNearlyEqual(ModelledTip, ExpectedTip, Tolerance));

        // Reference pose == full extension, so the stretch factor there must be 1.
        TestTrue(
            FString::Printf(TEXT("Arm %d is unscaled at full extension"), i),
            FMath::IsNearlyEqual(
                OctoArm::ArmChainLengthScale(Arm.ChainOriginOffset, Arm.RestChainLength, ExpectedTip),
                1.f, 0.002f));

        // ...and fully retracted, the tip lands on the body's own surface, which is
        // what stops a gap opening between hand and body.
        const float RetractedScale = OctoArm::ArmChainLengthScale(
            Arm.ChainOriginOffset, Arm.RestChainLength,
            OctoArm::HandCenterOffset(Defaults.SphereRadius, Defaults.ArmRadius, Defaults.ArmHalfHeight, 0.f));

        TestTrue(
            FString::Printf(TEXT("Arm %d's retracted tip sits on the body surface"), i),
            FMath::IsNearlyEqual(
                Arm.ChainOriginOffset + Arm.RestChainLength * RetractedScale,
                Defaults.SphereRadius, Tolerance));

        // The chain root is inside the body sphere, which is why retracting is a
        // stretch to ~0.1 rather than a stretch to 0.
        TestTrue(
            FString::Printf(TEXT("Arm %d's chain root is inside the body sphere (%.3f < %.1f)"),
                i, Arm.ChainOriginOffset, Defaults.SphereRadius),
            Arm.ChainOriginOffset < Defaults.SphereRadius);
        TestTrue(
            FString::Printf(TEXT("Arm %d's retracted scale is positive"), i),
            RetractedScale > 0.f);
    }

    // All eight arms are the same length — an asymmetric octopus would mean a
    // half-finished re-export, and the game math assumes symmetry.
    for (int32 i = 1; i < Arms.Num(); i++)
    {
        TestTrue(FString::Printf(TEXT("Arm %d's chain length matches arm 0's"), i),
            FMath::IsNearlyEqual(Arms[i].RestChainLength, Arms[0].RestChainLength, Tolerance));
        TestTrue(FString::Printf(TEXT("Arm %d's chain origin matches arm 0's"), i),
            FMath::IsNearlyEqual(Arms[i].ChainOriginOffset, Arms[0].ChainOriginOffset, Tolerance));
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoSkeletonBodyChainResolves,
    "PartyButtons.Octo.Skeleton.BodyChainResolves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoSkeletonBodyChainResolves::RunTest(const FString& Parameters)
{
    USkeletalMesh* Mesh = LoadOctoMesh();
    if (!TestNotNull(TEXT("SK_Okto loads"), Mesh))
    {
        return false;
    }

    FOctoBodyBones Body;
    if (!TestTrue(TEXT("The head chain resolves"), OctoSkeleton::BuildBodyBones(*Mesh, Body)))
    {
        return false;
    }

    TestTrue(TEXT("The resolved chain reports itself valid"), Body.IsValid());
    TestEqual(TEXT("Four bones: Body, Face, Head1, Head2"), Body.ChainIndices.Num(), 4);
    TestEqual(TEXT("A reference bone-space transform per bone"), Body.RefLocal.Num(), Body.ChainIndices.Num());
    TestEqual(TEXT("A reference component-space transform per bone"), Body.RefCS.Num(), Body.ChainIndices.Num());

    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    for (int32 i = 0; i < Body.ChainIndices.Num(); i++)
    {
        TestEqual(
            FString::Printf(TEXT("Chain entry %d is bone '%s'"), i, OctoSkeleton::BodyChainBoneNames[i]),
            RefSkeleton.GetBoneName(Body.ChainIndices[i]),
            FName(OctoSkeleton::BodyChainBoneNames[i]));
    }

    // The chain runs along component -X: straight out of the Y-Z play plane, toward the
    // camera. A re-export that swung it into the play plane would leave the head bending
    // along the axis the octopus travels on, which is not what the spring is tuned for.
    TestTrue(FString::Printf(TEXT("Rest direction is unit length (%.4f)"), Body.RestDir.Size()),
        FMath::IsNearlyEqual(Body.RestDir.Size(), 1.0, 0.001));
    TestTrue(
        FString::Printf(TEXT("The head points along -X (%s)"), *Body.RestDir.ToString()),
        Body.RestDir.Equals(FVector(-1.0, 0.0, 0.0), 0.001));

    // The head's collision sphere is drawn at the tip with the body's own radius, so a
    // chain much shorter than that radius would bury the head inside the body sphere.
    const FOctoTuning Defaults;
    TestTrue(
        FString::Printf(TEXT("The chain is longer than the body's radius (%.3f > %.1f)"),
            Body.ChainLength, Defaults.SphereRadius),
        Body.ChainLength > Defaults.SphereRadius);

    TestTrue(
        FString::Printf(TEXT("The tip sits at the chain's full length along the rest direction (%s)"),
            *Body.TipRestCS.ToString()),
        Body.TipRestCS.Equals(Body.RefCS[0].GetLocation() + Body.RestDir * Body.ChainLength, Tolerance));

    // The bend composes down the chain, so it has to BE a chain.
    for (int32 i = 1; i < Body.ChainIndices.Num(); i++)
    {
        TestEqual(
            FString::Printf(TEXT("'%s' is parented to '%s'"),
                OctoSkeleton::BodyChainBoneNames[i], OctoSkeleton::BodyChainBoneNames[i - 1]),
            RefSkeleton.GetParentIndex(Body.ChainIndices[i]),
            Body.ChainIndices[i - 1]);
    }

    // Nothing in the arm rig may live under the head chain, or bending the head would drag
    // an arm off the collider it is contractually pinned to.
    TArray<FOctoArmBones> Arms;
    if (OctoSkeleton::BuildArmBones(*Mesh, Arms))
    {
        for (int32 i = 0; i < Arms.Num(); i++)
        {
            for (const int32 ChainIndex : Body.ChainIndices)
            {
                TestTrue(
                    FString::Printf(TEXT("Arm %d's chain root is not parented into the head chain"), i),
                    RefSkeleton.GetParentIndex(Arms[i].ChainRootIndex) != ChainIndex);
                TestTrue(
                    FString::Printf(TEXT("Hand %d is not parented into the head chain"), i),
                    RefSkeleton.GetParentIndex(Arms[i].HandIndex) != ChainIndex);
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoSkeletonDumpRestPose,
    "PartyButtons.Octo.Skeleton.DumpRestPose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoSkeletonDumpRestPose::RunTest(const FString& Parameters)
{
    // Not an assertion — the readable record of what the imported asset actually
    // contains, so diagnosing a bad re-export doesn't need the editor open.
    USkeletalMesh* Mesh = LoadOctoMesh();
    if (!TestNotNull(TEXT("SK_Okto loads"), Mesh))
    {
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    const TArray<FTransform>  CS = OctoSkeleton::ComposeRefPoseComponentSpace(*Mesh);

    AddInfo(FString::Printf(TEXT("SK_Okto has %d bones"), RefSkeleton.GetNum()));

    for (int32 i = 0; i < RefSkeleton.GetNum(); i++)
    {
        const FTransform& Local = RefSkeleton.GetRefBonePose()[i];
        AddInfo(FString::Printf(
            TEXT("[%2d] %-12s parent=%-3d localT=(%8.3f,%8.3f,%8.3f) csT=(%8.3f,%8.3f,%8.3f)"),
            i, *RefSkeleton.GetBoneName(i).ToString(), RefSkeleton.GetParentIndex(i),
            Local.GetLocation().X, Local.GetLocation().Y, Local.GetLocation().Z,
            CS[i].GetLocation().X, CS[i].GetLocation().Y, CS[i].GetLocation().Z));
    }

    TArray<FOctoArmBones> Arms;
    OctoSkeleton::BuildArmBones(*Mesh, Arms);

    const FOctoTuning Defaults;
    AddInfo(FString::Printf(
        TEXT("Tuning: SphereRadius=%.1f ArmRadius=%.1f ArmHalfHeight=%.1f ArmMaxExtension=%.1f -> effective max=%.1f, reach=%.1f"),
        Defaults.SphereRadius, Defaults.ArmRadius, Defaults.ArmHalfHeight, Defaults.ArmMaxExtension,
        EffectiveMaxExtension(Defaults),
        OctoArm::HandCenterOffset(Defaults.SphereRadius, Defaults.ArmRadius, Defaults.ArmHalfHeight,
            EffectiveMaxExtension(Defaults))));

    for (int32 i = 0; i < Arms.Num(); i++)
    {
        const FOctoArmBones& Arm = Arms[i];
        AddInfo(FString::Printf(
            TEXT("Arm%d: restDir=(%6.3f,%6.3f,%6.3f) lengthAxis=(%6.3f,%6.3f,%6.3f) chainOrigin=%6.2f chainLen=%6.2f tip=%7.2f"),
            i + 1,
            Arm.RestDirection.X, Arm.RestDirection.Y, Arm.RestDirection.Z,
            Arm.LengthAxis.X, Arm.LengthAxis.Y, Arm.LengthAxis.Z,
            Arm.ChainOriginOffset, Arm.RestChainLength,
            Arm.ChainOriginOffset + Arm.RestChainLength));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
