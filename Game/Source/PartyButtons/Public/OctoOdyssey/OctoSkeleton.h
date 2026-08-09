#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

/**
 * OctoSkeleton — everything AOctoPawn needs to know about SK_Okto's arms,
 * measured out of the mesh's own REFERENCE skeleton rather than hard-coded.
 *
 * The rig contract SK_Okto is authored to:
 *
 *   Root
 *     +- Body -> Face -> Head1 -> Head2      (not driven yet — floppy body is a follow-up)
 *     +- Arm{N} -> Arm{N}_001 -> Arm{N}_002  (N = 1..8, three equal colinear segments)
 *     +- Hand{N}                             (parented to Root, NOT to the arm chain,
 *                                             and sitting exactly on the chain's tip)
 *
 * The reference pose is the octopus at FULL extension, at a zero arm-angle offset:
 * Arm1 points +Z, Arm3 +Y, Arm5 -Z, Arm7 -Y, evens interpolated — which is exactly
 * OctoArm::ArmDirectionLocal(N-1, 8, 0). The pawn supplies Tuning.ArmAngleOffsetDegrees
 * as a roll on the mesh COMPONENT instead of baking it into the asset.
 *
 * Nothing here is hard-coded to that geometry beyond the bone NAMES: the rest
 * direction, the bone-local length axis and the chain length are all measured, so a
 * re-export that changes the FBX axis convention or the modelled reach keeps working.
 * PartyButtons.Octo.Skeleton.* pins the measurements that the game math does care
 * about (that reach agrees with FOctoTuning, and that arm N points where arm N should).
 */
struct PARTYBUTTONS_API FOctoArmBones
{
    /** Index of Arm{N} — the chain bone the length scale is written to. */
    int32 ChainRootIndex = INDEX_NONE;

    /** Index of Hand{N} — positioned directly, since it hangs off Root rather than the chain. */
    int32 HandIndex = INDEX_NONE;

    /** Component-space unit direction this arm points in the reference pose. */
    FVector RestDirection = FVector::ZAxisVector;

    /**
     * The axis, in Arm{N}'s OWN bone space, that the chain runs along — measured as the
     * direction of Arm{N}_001's reference offset. SK_Okto imports with this on -Y.
     */
    FVector LengthAxis = FVector::XAxisVector;

    /** How far along RestDirection Arm{N} itself sits. Fixed: the chain root never moves. */
    float ChainOriginOffset = 0.f;

    /** Reference-pose distance from Arm{N} to Hand{N}, along RestDirection. */
    float RestChainLength = 0.f;

    /**
     * Reference component-space transform of Hand{N}'s PARENT, used to turn a
     * component-space hand target back into the bone-space transform the poseable
     * mesh stores. Captured rather than assumed identity so re-parenting the hands
     * under, say, Body would not silently mis-place them.
     */
    FTransform HandParentRestCS = FTransform::Identity;
};

namespace OctoSkeleton
{
    /**
     * Measure all OctoArm::NumArms arms out of Mesh's reference skeleton.
     *
     * Returns false and logs (LogPartyButtons, Warning) on the first arm with a missing
     * bone or a degenerate chain, leaving OutArms empty — callers are expected to treat
     * that as "don't pose the mesh" and leave it at its rest pose rather than to fail
     * hard, so a bad re-export costs you the animation, not the level.
     */
    PARTYBUTTONS_API bool BuildArmBones(const USkeletalMesh& Mesh, TArray<FOctoArmBones>& OutArms);

    /**
     * Reference-pose transform of every bone in component space, indexed like the
     * reference skeleton. Exposed for the tests, which check bones this struct does
     * not describe (the Body/Face/Head chain).
     */
    PARTYBUTTONS_API TArray<FTransform> ComposeRefPoseComponentSpace(const USkeletalMesh& Mesh);
}
