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
 *     +- Root1                               (an empty duplicate of Root at the origin,
 *                                             weighted to nothing — an export artifact, ignored)
 *     +- Body -> Face -> Head1 -> Head2      (the head, driven as a damped spring —
 *                                             see FOctoBodyBones and OctoBodySpring.h)
 *     +- Arm{N} -> Arm{N}_001 -> Arm{N}_002  (N = 1..8, three equal colinear segments)
 *     +- Hand{N}                             (parented to Root, NOT to the arm chain,
 *                                             and sitting exactly on the chain's tip)
 *
 * Note the body chain hangs off Root, not off Body's siblings — so bending the head
 * never moves an arm, which is what keeps the skeletal arms exactly on their colliders.
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

/**
 * SK_Okto's head chain — Body -> Face -> Head1 -> Head2 — measured out of the reference
 * skeleton the same way FOctoArmBones measures an arm.
 *
 * The chain runs along component -X (85.06cm of it), i.e. straight out of the Y-Z play
 * plane toward the camera, which sits at PlayPlaneX - CameraDistanceX looking +X. Nothing
 * below assumes that direction — it is measured — but it is why the head's deflection is
 * constrained to the Y-Z plane: a chain pointing down the camera's own axis can only
 * usefully swing across it.
 *
 * Bend is distributed across the first three entries; Head2 is the tip and carries no
 * rotation of its own (it is a leaf, and its scale is where the impact squash goes).
 */
struct PARTYBUTTONS_API FOctoBodyBones
{
    /** Reference-skeleton indices of Body, Face, Head1, Head2, in that order. Empty means "not resolved". */
    TArray<int32> ChainIndices;

    /** Reference bone-space transform of each chain bone — the pose to restore when the spring is off. */
    TArray<FTransform> RefLocal;

    /** Reference component-space transform of each chain bone. */
    TArray<FTransform> RefCS;

    /** Reference component-space transform of Body's PARENT (Root), needed to convert the chain root back to bone space. */
    FTransform RootParentRefCS = FTransform::Identity;

    /** Head2's component-space origin — also the centre of the head's collision sphere. */
    FVector TipRestCS = FVector::ZeroVector;

    /** Unit direction from the chain's base to its tip in the reference pose. SK_Okto imports with this on -X. */
    FVector RestDir = FVector::ZeroVector;

    /** Reference base-to-tip distance. The bend is angular, so this length is conserved. */
    float ChainLength = 0.f;

    bool IsValid() const { return ChainIndices.Num() == 4 && ChainLength > UE_KINDA_SMALL_NUMBER; }
};

namespace OctoSkeleton
{
    /**
     * The head chain's bone names, base to tip. Left as TCHAR literals rather than FNames
     * because a namespace-scope FName array would be constructed before the FName table
     * exists — the same reason BuildArmBones builds its names at the call site.
     */
    inline const TCHAR* const BodyChainBoneNames[] = { TEXT("Body"), TEXT("Face"), TEXT("Head1"), TEXT("Head2") };

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
     * Measure the Body -> Face -> Head1 -> Head2 chain out of Mesh's reference skeleton.
     *
     * Same fail-soft contract as BuildArmBones: a missing bone, a chain that isn't actually
     * parented in that order, or a degenerate length logs (LogPartyButtons, Warning) and
     * returns false with OutBody left empty. Callers treat that as "don't pose the head" and
     * render it rigid — a bad re-export costs the wobble, not the level. The two rigs are
     * measured independently, so one can fail without taking the other down.
     */
    PARTYBUTTONS_API bool BuildBodyBones(const USkeletalMesh& Mesh, FOctoBodyBones& OutBody);

    /**
     * Reference-pose transform of every bone in component space, indexed like the
     * reference skeleton. Exposed for the tests, which check bones this struct does
     * not describe (the Body/Face/Head chain).
     */
    PARTYBUTTONS_API TArray<FTransform> ComposeRefPoseComponentSpace(const USkeletalMesh& Mesh);
}
