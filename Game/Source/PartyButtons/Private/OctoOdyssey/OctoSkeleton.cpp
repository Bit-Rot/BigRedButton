#include "OctoOdyssey/OctoSkeleton.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "PartyButtons.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace OctoSkeleton
{

TArray<FTransform> ComposeRefPoseComponentSpace(const USkeletalMesh& Mesh)
{
    const FReferenceSkeleton& RefSkeleton = Mesh.GetRefSkeleton();
    const TArray<FTransform>& BoneSpace = RefSkeleton.GetRefBonePose();

    TArray<FTransform> ComponentSpace;
    ComponentSpace.SetNum(BoneSpace.Num());

    // A FReferenceSkeleton always lists a parent before its children, so one
    // forward pass suffices — no recursion, no revisiting.
    for (int32 i = 0; i < BoneSpace.Num(); i++)
    {
        const int32 Parent = RefSkeleton.GetParentIndex(i);
        ComponentSpace[i] = (Parent == INDEX_NONE)
            ? BoneSpace[i]
            : BoneSpace[i] * ComponentSpace[Parent];
    }

    return ComponentSpace;
}

bool BuildArmBones(const USkeletalMesh& Mesh, TArray<FOctoArmBones>& OutArms)
{
    OutArms.Reset();

    const FReferenceSkeleton& RefSkeleton = Mesh.GetRefSkeleton();
    const TArray<FTransform>& BoneSpace = RefSkeleton.GetRefBonePose();
    const TArray<FTransform>  CS        = ComposeRefPoseComponentSpace(Mesh);

    OutArms.Reserve(OctoArm::NumArms);

    for (int32 Arm = 0; Arm < OctoArm::NumArms; Arm++)
    {
        // Blender's "Arm1.001" imports as "Arm1_001" — the dot is not a legal FName
        // character, so the FBX pipeline substitutes an underscore.
        const int32 ChainRoot = RefSkeleton.FindBoneIndex(FName(*FString::Printf(TEXT("Arm%d"),     Arm + 1)));
        const int32 Mid       = RefSkeleton.FindBoneIndex(FName(*FString::Printf(TEXT("Arm%d_001"), Arm + 1)));
        const int32 Hand      = RefSkeleton.FindBoneIndex(FName(*FString::Printf(TEXT("Hand%d"),    Arm + 1)));

        if (ChainRoot == INDEX_NONE || Mid == INDEX_NONE || Hand == INDEX_NONE)
        {
            UE_LOG(LogPartyButtons, Warning,
                TEXT("OctoSkeleton: %s is missing arm %d's bones (Arm%d=%d, Arm%d_001=%d, Hand%d=%d). Not posing the mesh."),
                *Mesh.GetName(), Arm + 1, Arm + 1, ChainRoot, Arm + 1, Mid, Arm + 1, Hand);
            OutArms.Reset();
            return false;
        }

        FOctoArmBones Bones;
        Bones.ChainRootIndex = ChainRoot;
        Bones.HandIndex      = Hand;

        // The hand sits on the chain's tip in the reference pose, so its own position
        // IS the arm's direction — no need to reconstruct one from the arm angles.
        Bones.RestDirection = CS[Hand].GetLocation().GetSafeNormal();

        // Measured, not assumed: whichever local axis Arm{N}_001 is offset along is the
        // axis the chain runs down, whatever the exporter chose.
        Bones.LengthAxis = BoneSpace[Mid].GetLocation().GetSafeNormal();

        Bones.ChainOriginOffset = FVector::DotProduct(CS[ChainRoot].GetLocation(), Bones.RestDirection);
        Bones.RestChainLength   = FVector::DotProduct(CS[Hand].GetLocation() - CS[ChainRoot].GetLocation(), Bones.RestDirection);

        const int32 HandParent = RefSkeleton.GetParentIndex(Hand);
        Bones.HandParentRestCS = (HandParent == INDEX_NONE) ? FTransform::Identity : CS[HandParent];

        if (Bones.RestDirection.IsNearlyZero() ||
            Bones.LengthAxis.IsNearlyZero() ||
            Bones.RestChainLength <= UE_KINDA_SMALL_NUMBER)
        {
            UE_LOG(LogPartyButtons, Warning,
                TEXT("OctoSkeleton: %s arm %d is degenerate (restDir=%s, lengthAxis=%s, chainLength=%.3f). Not posing the mesh."),
                *Mesh.GetName(), Arm + 1, *Bones.RestDirection.ToString(), *Bones.LengthAxis.ToString(), Bones.RestChainLength);
            OutArms.Reset();
            return false;
        }

        OutArms.Add(Bones);
    }

    return true;
}

bool BuildBodyBones(const USkeletalMesh& Mesh, FOctoBodyBones& OutBody)
{
    OutBody = FOctoBodyBones();

    const FReferenceSkeleton& RefSkeleton = Mesh.GetRefSkeleton();
    const TArray<FTransform>& BoneSpace   = RefSkeleton.GetRefBonePose();
    const TArray<FTransform>  CS          = ComposeRefPoseComponentSpace(Mesh);

    const int32 NumChainBones = UE_ARRAY_COUNT(BodyChainBoneNames);

    OutBody.ChainIndices.Reserve(NumChainBones);
    OutBody.RefLocal.Reserve(NumChainBones);
    OutBody.RefCS.Reserve(NumChainBones);

    for (int32 i = 0; i < NumChainBones; i++)
    {
        const int32 Index = RefSkeleton.FindBoneIndex(FName(BodyChainBoneNames[i]));
        if (Index == INDEX_NONE)
        {
            UE_LOG(LogPartyButtons, Warning,
                TEXT("OctoSkeleton: %s is missing head-chain bone '%s'. Not posing the head."),
                *Mesh.GetName(), BodyChainBoneNames[i]);
            OutBody = FOctoBodyBones();
            return false;
        }

        // The bend composes down the chain, so the chain has to BE a chain — a re-export
        // that re-parented Head1 under Body would otherwise pose silently and wrongly.
        if (i > 0 && RefSkeleton.GetParentIndex(Index) != OutBody.ChainIndices[i - 1])
        {
            UE_LOG(LogPartyButtons, Warning,
                TEXT("OctoSkeleton: %s's '%s' is not parented to '%s'. Not posing the head."),
                *Mesh.GetName(), BodyChainBoneNames[i], BodyChainBoneNames[i - 1]);
            OutBody = FOctoBodyBones();
            return false;
        }

        OutBody.ChainIndices.Add(Index);
        OutBody.RefLocal.Add(BoneSpace[Index]);
        OutBody.RefCS.Add(CS[Index]);
    }

    const int32 RootParent = RefSkeleton.GetParentIndex(OutBody.ChainIndices[0]);
    OutBody.RootParentRefCS = (RootParent == INDEX_NONE) ? FTransform::Identity : CS[RootParent];

    OutBody.TipRestCS = OutBody.RefCS.Last().GetLocation();

    const FVector BaseToTip = OutBody.TipRestCS - OutBody.RefCS[0].GetLocation();
    OutBody.RestDir     = BaseToTip.GetSafeNormal();
    OutBody.ChainLength = BaseToTip.Size();

    if (OutBody.RestDir.IsNearlyZero() || OutBody.ChainLength <= UE_KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("OctoSkeleton: %s's head chain is degenerate (restDir=%s, length=%.3f). Not posing the head."),
            *Mesh.GetName(), *OutBody.RestDir.ToString(), OutBody.ChainLength);
        OutBody = FOctoBodyBones();
        return false;
    }

    return true;
}

} // namespace OctoSkeleton
