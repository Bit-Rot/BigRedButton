#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "OctoTuning.generated.h"

/**
 * FOctoTuning
 *
 * THE single source of truth for every OctoOdyssey knob that affects game feel.
 * AOctoPawn, AOctoCamera and AOctoGameMode all read their numbers from one of
 * these instead of owning their own EditDefaultsOnly properties, so a value
 * exists in exactly one place and the dev tuning menu (Tab — see
 * AOctoGameMode::OnDevMenuToggle) can edit all of them uniformly.
 *
 * The member initialisers below ARE the shipping defaults. That is what makes
 * the round trip work: tune in-game, hit Accept, and the JSON line dumped to
 * the log (OctoTuning::ToJsonString) has one key per field named exactly like
 * the field, ready to be pasted back over these initialisers.
 *
 * Adding a knob is two edits: a field here, and a row in the descriptor table
 * in OctoTuning.cpp. The menu, the sliders and the JSON all pick it up.
 *
 * A tuned instance lives in UOctoTuningSubsystem (a GameInstance subsystem, so
 * it survives the OpenLevel that Accept triggers). Actors keep their own copy
 * so they still work with no subsystem present — e.g. an editor viewport, or
 * L_GameC opened with no GameInstance-backed session.
 */
USTRUCT()
struct PARTYBUTTONS_API FOctoTuning
{
    GENERATED_BODY()

    // ---- Geometry ---------------------------------------------------------
    //
    // Baked into the pawn's components before their physics state is created
    // (AOctoPawn::OnConstruction → RebuildGeometry). Cannot be previewed live:
    // resizing a shape after it has welded into the body's shape union risks
    // silently breaking the weld — see AOctoPawn's class comment.

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float SphereRadius = 40.f;

    /** Arm CAPSULE radius, and the hand sphere's radius. The shaft mesh is slimmer — see ArmMeshRadiusScale. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmRadius = 20.f;

    /**
     * Arm cylinder MESH radius as a fraction of ArmRadius. Purely visual — the capsule
     * collider is always the full ArmRadius, so slimming the shaft never touches
     * physics. The hand sphere stays at full ArmRadius, which is what gives the arm its
     * thin-limb-into-ball silhouette.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmMeshRadiusScale = 0.75f;

    /** UE capsule half-height INCLUDES the hemisphere caps (total capsule length = 2*ArmHalfHeight). */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmHalfHeight = 45.f;

    /**
     * Clamped in TickArm to OctoArm::MaxSafeExtension(SphereRadius, ArmRadius, ArmHalfHeight)
     * as a hard safety backstop.
     *
     * At the defaults above the two coincide exactly: MaxSafeExtension(40, 20, 45) ==
     * 2*45 - 20 == 70, so the effective hand-centre reach is SphereRadius + 70 == 110 —
     * which is the reach SK_Okto is modelled at (PartyButtons.Octo.Skeleton.ReachMatchesTuning).
     * Raising THIS value alone therefore does nothing but add dead range above the clamp;
     * to lengthen the stroke, raise ArmHalfHeight with it and re-export the mesh.
     * PartyButtons.Octo.ArmMath.ExtensionInvariantsHold fails if the two drift apart.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmMaxExtension = 70.f;

    /** Angle of arm 0 from +Z; arm i sits at AngleOffset + 360*i/8. 22.5 keeps no arm pointing straight down. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmAngleOffsetDegrees = 22.5f;

    /** Shrinks the swept collision test so an arm already resting on a surface doesn't self-trigger. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmSweepSkin = 1.f;

    // ---- Arms -------------------------------------------------------------

    /**
     * How fast an arm extends — and therefore, under the absolute-arm model,
     * exactly how fast a planted arm throws the body away from the surface
     * (see AOctoPawn::TickArm). Ballistic apex is ExtendSpeed^2 / (2*g): 1200
     * gives ~7.3 m, about nine body diameters.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Arms")
    float ExtendSpeed = 1200.f;

    /** Kept in proportion to ExtendSpeed so the re-fire cadence doesn't feel sticky. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Arms")
    float RetractSpeed = 900.f;

    /**
     * How hard a planted arm resists the body sliding ACROSS its contact, as a
     * fraction of the tangential velocity killed per frame (0 = none, 1 = all).
     *
     * This is the knob for "push-off feels slippery". The axial push-off cannot
     * help: OctoArm::PushOffSpeedDeficit deliberately leaves motion
     * perpendicular to the arm untouched, so without this an arm has no way to
     * arrest a sideways slide no matter what the surface friction is. Contact
     * friction proper (see SurfaceFriction) barely bites either, because a
     * planted arm's collider is TELEPORTED each tick rather than solved, so
     * Chaos sees a moved shape carrying no shape velocity.
     *
     * Defaults to 1 — a planted arm kills ALL tangential motion, which is what
     * playtesting settled on. Drop it toward 0 to make push-offs slide.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Arms")
    float ArmGripFraction = 1.f;

    /**
     * Apply the push-off impulse at the contact point (true) or at the centre of
     * mass (false). The contact point is what makes a corner push tumble the
     * body; the centre of mass gives a cleaner, less chaotic launch.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Arms")
    bool bPushAtImpactPoint = true;

    /**
     * Include ECC_PhysicsBody in the arm sweep. Off by default, matching the
     * original behaviour: with only WorldStatic/WorldDynamic queried, arms slide
     * straight through physics props instead of pushing off them.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Arms")
    bool bSweepPhysicsBodies = false;

    // ---- Body -------------------------------------------------------------
    //
    // SK_Okto's head chain (Body -> Face -> Head1 -> Head2) driven as a damped spring —
    // see OctoBodySpring.h for the model and AOctoPawn::TickBodySpring for the wiring.
    //
    // Every value here is purely visual: the head is a passenger that the world can stop
    // but that never pushes the physics body back. So unlike the geometry block above,
    // all of it is live-appliable and previews mid-round.

    /** Master switch. Off restores the reference pose exactly — the head goes rigid again. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    bool bBodySpring = true;

    /** How fast the head oscillates. Higher is tighter and jellier; lower is a slow, heavy lope. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodySpringFrequencyHz = 2.5f;

    /**
     * Damping RATIO, not a coefficient: 1 is critical (no overshoot at all), below 1 rings.
     * The default sits below 1 on purpose — the overshoot IS the flail when the
     * octopus slams to a halt, and a critically damped head just looks stiff.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodySpringDamping = 0.76f;

    /** Scales the deflection the spring produces before it is posed. The "how floppy" dial. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodyLagScale = 2.45f;

    /**
     * How far the head TRAILS a travelling octopus, in cm per 100 cm/s of speed — as if it
     * were dragging through air.
     *
     * This is a different feel from the spring lag above, and the two are worth thinking
     * about separately. The spring responds to ACCELERATION: it flings the head about when
     * the octopus changes direction and then lets it centre again during a steady glide.
     * This responds to SPEED: it holds the head consistently swept back for as long as the
     * octopus is moving, and points it where the octopus has been. Turn it up (and the
     * frequency up with it) for a head that reads as light and air-dragged rather than heavy
     * and pendulum-like.
     *
     * The resulting trail is independent of BodySpringFrequencyHz and BodySpringDamping by
     * construction — see OctoBody::DragAccel — so stiffening the spring to kill the wobble
     * does not also flatten the drag.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodyAirDrag = 2.f;

    /** Ceiling on head deflection, in cm, before the bend is computed. 0 disables the clamp. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodyMaxDeflection = 60.f;

    /** Fraction of WorldGravityZ applied to the head particle, so it droops instead of hanging level. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodyGravityScale = 0.25f;

    /**
     * How the bend is shared out along the chain. 0 spreads it evenly; 1 crowds it into the
     * joints nearest the head, which keeps the chain's BASE nearly still — and the base is
     * the part of the body the eight rigid arms emerge from, so this is the knob that stops
     * the skin tearing at the arm roots.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodyBendTaper = 0.55f;

    /**
     * Let the head bone ride its parent rigidly instead of bending on its own.
     *
     * On (the default) the bend lands on Body/Face/Head1 and the head travels as one solid
     * piece on a bending neck. Off, every share shifts one bone down the chain and Head2 is
     * left holding the remainder — at the default taper that is 67% of the whole bend, about
     * 37 degrees, applied at the very tip inside the head mesh, where it reads as the head
     * shearing rather than the neck bending. Kept as a switch purely so the difference can be
     * seen back to back; there is no reason to ship it off.
     *
     * Squash and stretch is unaffected either way — that rides the head bone's SCALE, which
     * neither setting touches.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    bool bHeadTipRigid = true;

    /** Hard limit on the total bend. The real cap on how far the head can throw. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float BodyMaxBendDegrees = 55.f;

    /** Sweep the head sphere (radius SphereRadius) against the world so it cannot sink into geometry. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    bool bHeadCollision = true;

    /**
     * Added to the head sphere's X before the sweep. 0 tests it where the bone actually is.
     *
     * This exists because the head chain points along -X, ~85cm out of the play plane and
     * TOWARD the camera, while the course is authored at X in [0, 400] with its front face
     * coplanar with the play plane. At offset 0 the head therefore sweeps through empty space
     * in front of the level and hits nothing until the course is widened forward — which in
     * turn puts geometry between the camera and the octopus. Set this to ~85 to test the head
     * in the play plane instead, exactly where BodySphere already collides, and the head
     * collides against the course as authored at the cost of the collision being a plane off
     * from the visual. Which trade is right is a look-at-it call, so it is a slider.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadCollisionOffsetX = 0.f;

    /** How much of the head's inward speed survives an impact, reversed. 0 is a dead stop. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadCollisionRestitution = 0.8f;

    /** Fraction of the head's ACROSS-surface speed killed on impact. 1 makes the head stick and drag. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadCollisionFriction = 0.4f;

    /** Squash and stretch the head on hard impacts. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    bool bHeadSquash = true;

    /** Deepest squash, as a fraction: 0.35 flattens the head to 65% along the impact normal. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadSquashMax = 0.35f;

    /** Impact speed (cm/s) that produces HeadSquashMax. Anything faster is clamped to it. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadSquashFullSpeed = 1200.f;

    /** How fast the squash wobbles out. Deliberately well above the body spring — a jiggle, not a lope. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadSquashFrequencyHz = 6.f;

    /** Damping ratio for that wobble. Low enough to bounce two or three times before settling. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Body")
    float HeadSquashDamping = 0.25f;

    // ---- Physics ----------------------------------------------------------

    /**
     * Fixed mass regardless of arm extension — every arm move triggers
     * UpdateMassProperties; without this override total mass would breathe.
     *
     * Does NOT affect launch height: TickArm scales its push impulse by this
     * value precisely so mass cancels out of both the linear and the angular
     * result. Mass only governs how the octopus responds to contacts.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float BodyMassKg = 40.f;

    /** Kept low — damping high enough to matter visibly eats the top of a launch arc. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float LinearDamping = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float AngularDamping = 0.5f;

    /** Backstop on stacked pushes (several arms planted at once), not on a single arm — one arm can only ever reach ExtendSpeed. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float MaxLaunchSpeed = 2000.f;

    /**
     * Angular counterpart to MaxLaunchSpeed. A push applied at the contact point
     * (see bPushAtImpactPoint) can spin the body hard off a corner, and
     * AngularDamping alone takes a long time to bleed that off. 0 disables the clamp.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float MaxAngularSpeedDeg = 720.f;

    /** Per-body backstop in case the swept clamp is ever bypassed. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float MaxDepenetrationVelocity = 150.f;

    /** Continuous collision detection — the body moves fast enough to tunnel without it. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    bool bUseCCD = true;

    /** World gravity for the course, applied to AWorldSettings::GlobalGravityZ. UE's default is -980. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float WorldGravityZ = -980.f;

    // ---- Surface ----------------------------------------------------------
    //
    // Before this existed the project had no UPhysicalMaterial at all, so every
    // contact ran on the Chaos defaults (Friction 0.7, Restitution 0.3). These
    // defaults reproduce that exactly; the point is that they are now reachable.

    /** Apply the values below as a physical material override on the body and all 8 arm colliders. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Surface")
    bool bOverridePhysicalMaterial = true;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Surface")
    float SurfaceFriction = 0.7f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Surface")
    float SurfaceStaticFriction = 0.7f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Surface")
    float SurfaceRestitution = 0.3f;

    // ---- Visual -----------------------------------------------------------

    /**
     * Draw the original prototype shapes (body sphere, 8 arm cylinders, 8 hand
     * spheres) alongside SK_Okto instead of hiding them.
     *
     * They are kept, not deleted, precisely so this toggle exists: the prototype
     * meshes are drawn from the SAME OctoArm:: offsets the colliders use, so
     * switching them on is the direct visual check that the skeletal mesh is
     * tracking collision. Purely a render flag — it touches no physics, which is
     * why (unlike the geometry above) it is live-appliable.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Visual")
    bool bShowPrototypeMeshes = false;

    // ---- Camera -----------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float CameraDistanceX = 2000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float CameraHeightOffset = 150.f;

    /** How far ahead (+Y) of the octopus the camera sits. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float LeadY = 250.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float MinCameraZ = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float FollowInterpSpeed = 4.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float CameraFieldOfView = 55.f;

    // ---- Course -----------------------------------------------------------

    /**
     * World X the octopus is pinned to by its DOF lock, and the plane the camera
     * frames. ONE value read by both AOctoGameMode and AOctoCamera — they used to
     * hold independent copies that silently desynced if you edited only one.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Course")
    float PlayPlaneX = 0.f;

    /** Tutorial auto-dismiss deadline (APartyMinigameGameMode::TutorialMaxSeconds). */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Course")
    float TutorialMaxSeconds = 5.f;
};

/** Which member of FOctoTuning a descriptor row points at. */
enum class EOctoParamKind : uint8
{
    Float,
    Bool,
};

/**
 * One row of the tuning descriptor table — everything the dev menu and the JSON
 * dump need to handle a field generically.
 *
 * The ranges live HERE rather than in UPROPERTY meta=(UIMin/UIMax) on purpose:
 * FProperty metadata is WITH_EDITOR-only and is stripped from a Development
 * standalone build, which would leave the sliders rangeless in exactly the
 * configuration the arcade cabinet runs.
 */
struct PARTYBUTTONS_API FOctoTuningParam
{
    /** Field name — also the JSON key and the menu row label. */
    const TCHAR* Name = nullptr;

    /** Section heading in the menu. Rows are listed in table order, so keep categories contiguous. */
    const TCHAR* Category = nullptr;

    EOctoParamKind Kind = EOctoParamKind::Float;

    /** Exactly one of these is non-null, per Kind. */
    float FOctoTuning::* FloatMember = nullptr;
    bool  FOctoTuning::* BoolMember  = nullptr;

    /** Slider range and the amount one Left/Right press moves the value. Bools use 0..1 step 1. */
    float Min  = 0.f;
    float Max  = 1.f;
    float Step = 0.1f;

    /**
     * True for values baked in at spawn (geometry, play plane). The menu marks
     * these rows and skips them when applying a live preview — they only take
     * effect on Accept, which reloads the course.
     */
    bool bRequiresRestart = false;
};

namespace OctoTuning
{
    /** The descriptor table, in menu display order. */
    PARTYBUTTONS_API TArrayView<const FOctoTuningParam> GetParams();

    /**
     * One JSON object with a key per table row, named exactly like the
     * FOctoTuning field, so the dumped line pastes straight back over the
     * struct's member initialisers. Hand-rolled rather than going through
     * JsonUtilities: it keeps this header dependency-free and unit-testable.
     */
    PARTYBUTTONS_API FString ToJsonString(const FOctoTuning& Tuning);

    /** Current value mapped onto [0,1] across [Min,Max]. Bools read as 0 or 1. */
    PARTYBUTTONS_API float GetNormalized(const FOctoTuning& Tuning, const FOctoTuningParam& Param);

    /** Inverse of GetNormalized. Alpha is clamped to [0,1]; bools take the nearer end. */
    PARTYBUTTONS_API void SetNormalized(FOctoTuning& Tuning, const FOctoTuningParam& Param, float Alpha);

    /** Move a value one Step (Direction -1 or +1), clamped to range. Bools simply flip. */
    PARTYBUTTONS_API void Nudge(FOctoTuning& Tuning, const FOctoTuningParam& Param, int32 Direction);

    /** The value as it appears in the menu's right-hand column ("900.0", "On"). */
    PARTYBUTTONS_API FString FormatValue(const FOctoTuning& Tuning, const FOctoTuningParam& Param);

    // ---- Persistence -------------------------------------------------------
    //
    // Both of these drive a LOCAL FConfigFile rather than the global GConfig
    // cache, for two reasons that each cost a debugging session to find:
    //
    //   1. GConfig->SetFloat(..., Filename) silently does nothing when Filename
    //      has never been loaded — there is no cached FConfigFile to write into,
    //      so the value is dropped, the file stays un-dirtied, and the
    //      subsequent Flush returns SUCCESS having written nothing.
    //   2. GConfig caches by filename for the life of the process. In the editor
    //      that means a file written during one PIE session would be re-read from
    //      the stale in-memory copy on the next, so tuning would appear to
    //      "not persist" until the editor was restarted.
    //
    // A local FConfigFile has neither problem: it reads from disk every time and
    // writes the file it was handed.

    /** Section name inside the ini. One flat section — categories are a menu concern. */
    PARTYBUTTONS_API extern const TCHAR* const IniSection;

    /**
     * Write every field to IniPath. Returns false if the file could not be
     * written — callers MUST surface that rather than assume success, which is
     * exactly the trap FConfigFile::Write sets by returning true when it
     * silently skips a non-dirty write.
     */
    PARTYBUTTONS_API bool SaveToIni(const FOctoTuning& Tuning, const FString& IniPath);

    /**
     * Overlay IniPath onto Tuning, which should already hold the defaults.
     * Fields absent from the file are left alone, so adding a tunable never needs
     * a migration and deleting a line is a per-field reset. Values are clamped to
     * their declared range — a hand-edited file is the easiest way to end up with
     * something the sliders can't represent.
     *
     * Returns the number of fields the file overrode, and appends "Name=Value"
     * for each to OutOverridden when it is non-null (for logging).
     */
    PARTYBUTTONS_API int32 LoadFromIni(FOctoTuning& Tuning, const FString& IniPath, FString* OutOverridden = nullptr);
}
