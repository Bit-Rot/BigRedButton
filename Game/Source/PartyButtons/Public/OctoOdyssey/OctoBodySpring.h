#pragma once

#include "CoreMinimal.h"
#include "OctoOdyssey/OctoSkeleton.h"

/**
 * OctoBody — pure, world-free math for SK_Okto's floppy head.
 *
 * Same shape as OctoArm (OctoArmMath.h): free functions with no engine-object
 * dependency, so the whole spring can be unit-tested without a world, an actor or a
 * skeletal mesh. AOctoPawn::TickBodySpring is the only caller.
 *
 * The model is one particle on a damped spring, anchored to the head's rest position and
 * dragged along by the body. Everything the design asks for falls out of that: it lags
 * while the octopus accelerates, it overshoots and rings when the octopus stops dead, and
 * a world sweep on the particle gives collision. Nothing here integrates the body itself —
 * the head is a passenger and never pushes back (see AOctoPawn's class comment for why the
 * launch model must stay untouched).
 */
namespace OctoBody
{
    /**
     * Longest step the spring is ever integrated over. Semi-implicit Euler is stable while
     * omega*dt stays well under 2; at the top of BodySpringFrequencyHz's slider (8Hz,
     * omega ~= 50) this keeps the product near 0.21, which leaves room for a stiffer retune
     * without anyone having to remember this constant exists.
     */
    inline constexpr float MaxSubstep = 1.f / 240.f;

    /** Hard cap on substeps per frame, so a hitch costs a bounded amount of work. */
    inline constexpr int32 MaxSubstepsPerFrame = 32;

    /**
     * How much real time a single frame may advance the spring. A hitch longer than this
     * advances the simulation by less than wall-clock time, which is deliberate: the
     * alternative is the head catching up in one enormous, visibly wrong lurch.
     */
    inline constexpr float MaxFrameTime = 0.1f;

    /** The head particle. Position and velocity are WORLD space. */
    struct PARTYBUTTONS_API FHeadSpringState
    {
        FVector Position = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;

        /** False until the first Reset — StepSpring snaps rather than springing from the origin. */
        bool bInitialized = false;

        void Reset(const FVector& AtPosition)
        {
            Position     = AtPosition;
            Velocity     = FVector::ZeroVector;
            bInitialized = true;
        }
    };

    /**
     * Advance the head particle one frame toward a MOVING anchor.
     *
     * Damping acts on velocity RELATIVE to the anchor, which is the whole reason this reads
     * as a floppy body: a spring anchored to something that is itself moving lags behind
     * acceleration and overshoots on a sudden stop. Damping against absolute world velocity
     * would instead drag the head toward world-stationary, so the octopus would trail a head
     * that fights every constant-speed glide.
     *
     * DampingRatio is exactly that — a ratio, not a coefficient. 1 is critical (no
     * overshoot), below 1 rings, above 1 crawls. ExtraAccel is a constant world acceleration
     * applied on top (AOctoPawn feeds it a fraction of gravity, for droop).
     *
     * TargetVelocity must be the anchor's AVERAGE velocity over the frame —
     * (Target - PreviousTarget) / DeltaSeconds — not an instantaneous one. The integrator
     * relies on Target - TargetVelocity * DeltaSeconds being exactly where the anchor
     * started, and works in the anchor's frame on that basis; see the implementation for why
     * stepping the anchor forward instead lets the head drift past the body it should trail.
     *
     * Steady behaviour worth knowing when tuning: a constant-velocity glide produces no
     * deflection at all (unless DragAccel is fed in through ExtraAccel — that is exactly
     * what it is for), and a constant acceleration a settles at a lag of a/omega^2 — so lag
     * scales with acceleration, and softening BodySpringFrequencyHz by half quadruples it.
     *
     * Substeps internally at MaxSubstep and clamps the frame to MaxFrameTime, so neither a
     * stiff tuning nor a frame hitch can blow the integrator up. Uninitialised state snaps to
     * the target instead of springing to it from wherever the struct happened to be zeroed.
     */
    PARTYBUTTONS_API void StepSpring(
        FHeadSpringState& State,
        const FVector& Target,
        const FVector& TargetVelocity,
        float FrequencyHz,
        float DampingRatio,
        const FVector& ExtraAccel,
        float DeltaSeconds);

    /**
     * Condition a raw head deflection for posing: drop the X component and clamp the
     * magnitude to MaxDeflection.
     *
     * X goes because the octopus is pinned to the Y-Z plane (EDOFMode::YZPlane) and the head
     * chain points straight down the camera's view axis — an X deflection would read as the
     * head growing or shrinking rather than as motion. MaxDeflection <= 0 disables the clamp
     * rather than collapsing the head, so a zeroed slider can't look like a bug.
     */
    PARTYBUTTONS_API FVector ConstrainDeflection(const FVector& Deflection, float MaxDeflection);

    /**
     * Each joint's share of the total bend, base first, summing to 1.
     *
     * Taper 0 spreads the bend evenly. Taper 1 pushes it toward the joints nearest the head,
     * which is what keeps the chain's base — the part of the body the eight arms emerge
     * from — nearly still, so the skin does not tear where a rigid arm meets a moving body.
     * Weights are non-decreasing for every taper.
     */
    PARTYBUTTONS_API void BendWeights(int32 NumJoints, float Taper, TArray<float>& OutWeights);

    /**
     * Acceleration that makes the head TRAIL a travelling body, as if it were dragging
     * through air — as opposed to the pendulum lag StepSpring already produces, which
     * responds to ACCELERATION and vanishes the moment the octopus settles into a steady
     * glide. Feed the result to StepSpring's ExtraAccel.
     *
     * TrailPer100Speed is in cm of steady trail per 100 cm/s of travel, and it means exactly
     * that: the settled offset is TrailPer100Speed * Speed / 100, opposite the direction of
     * travel, and INDEPENDENT of FrequencyHz and DampingRatio. Retuning the spring's
     * stiffness therefore does not move the trail, which is what lets "how far the head
     * drags behind" and "how bouncy the head is" be tuned against each other rather than
     * fought over. See the implementation for why the Omega^2 factor is what buys that.
     *
     * Deliberately driven by the ANCHOR's velocity rather than the head's own. Real drag
     * would act on the head's absolute velocity, but the part of that which comes from the
     * head's motion RELATIVE to the body is just damping, and DampingRatio already owns
     * that dial. Splitting them keeps one knob for "trails while travelling" and another for
     * "stops wobbling", instead of two knobs that each do half of both.
     */
    PARTYBUTTONS_API FVector DragAccel(const FVector& AnchorVelocity, float TrailPer100Speed, float FrequencyHz);

    /**
     * Bone-space transforms for the whole head chain, swinging its tip toward TargetTipCS
     * (component space). One entry per FOctoBodyBones::ChainIndices entry, in the same order.
     *
     * THE BEND IS ANGULAR, SO THE CHAIN IS INEXTENSIBLE. The tip lands on the arc at
     * Base + WantDir * ChainLength, which is short of TargetTipCS by however much the
     * deflection lengthened the reach. That is correct for a chain of rigid bones and is not
     * a bug to be "fixed" — MaxBendDegrees is the real limit on how far the head can throw,
     * and BodyMaxDeflection shapes the response below it. (Adding a length scale so the tip
     * actually reaches would be a separate, deliberate change.)
     *
     * Rotations are distributed by BendWeights and composed down the chain, so the tip's
     * ORIGIN always receives the full rotation. The whole thing is computed in component
     * space and converted back via GetRelativeTransform, which is exact against the
     * composition rule in OctoSkeleton::ComposeRefPoseComponentSpace.
     *
     * bRigidTip decides whether the tip bone does any bending OF ITS OWN. With it set, the
     * weights fall on Body/Face/Head1 and the tip inherits Head1's orientation unchanged —
     * the head travels as one rigid piece on a bending neck, which is what a head should do.
     * Clear it and every weight shifts one bone down the chain: Body stops rotating entirely
     * and the tip is left holding the remainder, which at the default taper is 73% of the
     * whole bend concentrated at the very tip, inside the head mesh. That reads as the head
     * shearing rather than the neck bending, so it is off by default and kept only as an A/B.
     *
     * Either way the tip's SCALE is untouched here — the impact squash owns that.
     *
     * Reference scale is preserved on every bone — the impact squash multiplies into the
     * tip's scale afterwards, and would be lost if this overwrote it.
     *
     * OutTipCS, when non-null, receives the tip's posed COMPONENT-space transform, which is
     * the frame the squash normal has to be expressed in.
     */
    PARTYBUTTONS_API void BuildChainBend(
        const FOctoBodyBones& Bones,
        const FVector& TargetTipCS,
        float MaxBendDegrees,
        float Taper,
        bool bRigidTip,
        TArray<FTransform>& OutBoneSpace,
        FTransform* OutTipCS = nullptr);

    /**
     * Squash-and-stretch scale for the head, given the impact normal expressed in the head
     * bone's OWN frame: compress along the normal, bulge across it.
     *
     * Axis-aligned, because FTransform can only carry an axis-aligned scale. That is exact
     * for the impacts that actually happen — the head bone's local axes line up with
     * component +-X/+-Y/+-Z, and the octopus only moves in the Y-Z plane, so floors and walls
     * hit square on an axis. A 45-degree impact degrades to a roughly uniform squash rather
     * than a true diagonal one. The alternative (rotate the leaf bone to align an axis with
     * the normal, then scale) buys the diagonal case at the cost of visibly twisting the face
     * and its eye materials, so it is deliberately not taken here.
     *
     * Amount 0 returns unit scale exactly. Results are clamped so a wild tuning cannot
     * invert the head inside out.
     */
    PARTYBUTTONS_API FVector SquashScale(const FVector& NormalInBoneSpace, float Amount);

    /**
     * Ring the squash amount back down to 0 as a damped oscillator, so a hard landing
     * wobbles out instead of snapping back. Same substepping and stability guarantees as
     * StepSpring.
     */
    PARTYBUTTONS_API void StepSquash(
        float& Amount,
        float& Velocity,
        float FrequencyHz,
        float DampingRatio,
        float DeltaSeconds);
}
