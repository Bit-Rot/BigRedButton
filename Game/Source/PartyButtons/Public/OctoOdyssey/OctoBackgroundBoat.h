#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoBackgroundBoat.generated.h"

class AWaterBody;
class USplineComponent;
class UStaticMeshComponent;

/** Pure path math for AOctoBackgroundBoat, kept world-free so it can be unit tested. */
namespace OctoBoat
{
    /**
     * Map an unbounded travelled distance onto a spline of length SplineLength.
     * Closed loops wrap; open splines ping-pong end to end, and bOutBackward says
     * which leg of the ping-pong the boat is on (always false for a loop).
     */
    PARTYBUTTONS_API float WrapDistance(float Distance, float SplineLength, bool bClosedLoop, bool& bOutBackward);
}

/**
 * AOctoBackgroundBoat
 *
 * Set dressing for OctoOdyssey: a boat that sails around a spline and rides the
 * ocean. No collision and no gameplay.
 *
 * Components:
 *   Root       fixed; the actor transform. Move this to move the whole route.
 *   Path       closed spline (defaults to a ~30 m circle), attached to Root so it
 *              stays put while the boat moves. Edit its points per instance.
 *   BoatPivot  the moving part. Driven in world space every tick: position along
 *              Path, Z and tilt from the water.
 *   Hull       static mesh under BoatPivot. Its relative transform is the place
 *              to fix a mesh that doesn't face +X or sits at the wrong waterline.
 *              A skeletal boat later can go under BoatPivot too (BP subclass).
 *
 * Water: each tick samples the WaterBody surface (waves included) at the bow,
 * stern, port and starboard of a HullLength x HullWidth footprint. The average
 * sets the height, the differences set pitch and roll. With no water body, or
 * off its edge, the spline's own Z is the water line. The CPU wave evaluation
 * matches the rendered Gerstner waves, but the ocean mesh's LOD may not, so the
 * smoothing speeds exist to hide the mismatch rather than for realism.
 *
 * A procedural sine bob/pitch/roll is layered on top, so the boat still moves
 * on flat water. Phases are seeded from the actor name so several boats placed
 * with the same settings don't bob in lockstep.
 */
UCLASS()
class PARTYBUTTONS_API AOctoBackgroundBoat : public AActor
{
    GENERATED_BODY()

public:
    AOctoBackgroundBoat();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return bAnimateInEditor; }

protected:
    virtual void BeginPlay() override;

    // ---- Path ----------------------------------------------------------------

    /** Sailing speed along the spline, cm/s. 0 parks the boat at StartPhase (it still bobs). */
    UPROPERTY(EditAnywhere, Category = "Boat|Path", meta = (ClampMin = "0", UIMin = "0", UIMax = "2000", Units = "CentimetersPerSecond"))
    float Speed = 250.f;

    /** Sail the spline backwards (against its point order). */
    UPROPERTY(EditAnywhere, Category = "Boat|Path")
    bool bReverse = false;

    /** Where on the spline the boat starts, as a fraction of its length. Use to spread boats that share a route shape. */
    UPROPERTY(EditAnywhere, Category = "Boat|Path", meta = (ClampMin = "0", ClampMax = "1"))
    float StartPhase = 0.f;

    /** Roll, degrees, per degree/second of turn rate. Positive leans into the turn, negative heels outward. */
    UPROPERTY(EditAnywhere, Category = "Boat|Path", meta = (UIMin = "-1", UIMax = "1"))
    float BankPerTurnRate = 0.25f;

    /** Clamp on the turn bank, degrees. */
    UPROPERTY(EditAnywhere, Category = "Boat|Path", meta = (ClampMin = "0", UIMax = "30", Units = "Degrees"))
    float MaxBank = 8.f;

    // ---- Water ---------------------------------------------------------------

    /** Ride the water surface. Off = the spline's Z is the water line (the sway below still applies). */
    UPROPERTY(EditAnywhere, Category = "Boat|Water")
    bool bFollowWater = true;

    /** Water body to ride. Empty = the level's ocean (else the first water body found). */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (EditCondition = "bFollowWater"))
    TObjectPtr<AWaterBody> WaterBody;

    /** Added to the sampled water height. Negative sinks the boat deeper. The Hull's relative Z does the same per-mesh. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (Units = "Centimeters"))
    float WaterLineOffset = 0.f;

    /** Bow-to-stern distance between the wave samples. Roughly the hull length; longer = calmer pitch. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (ClampMin = "1", Units = "Centimeters", EditCondition = "bFollowWater"))
    float HullLength = 600.f;

    /** Port-to-starboard distance between the wave samples. Wider = calmer roll. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (ClampMin = "1", Units = "Centimeters", EditCondition = "bFollowWater"))
    float HullWidth = 250.f;

    /** Scales how far the waves lift and drop the boat. 1 = follow exactly. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (ClampMin = "0", UIMax = "2", EditCondition = "bFollowWater"))
    float WaveHeightScale = 1.f;

    /** Scales the pitch/roll the waves induce. 1 = match the surface slope. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (ClampMin = "0", UIMax = "2", EditCondition = "bFollowWater"))
    float WaveTiltScale = 1.f;

    /** Interp speed toward the target height. Lower = heavier, laggier boat. 0 = no smoothing. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (ClampMin = "0", UIMax = "20"))
    float HeightSmoothing = 4.f;

    /** Interp speed toward the target pitch/roll (waves + bank). 0 = no smoothing. */
    UPROPERTY(EditAnywhere, Category = "Boat|Water", meta = (ClampMin = "0", UIMax = "20"))
    float TiltSmoothing = 3.f;

    // ---- Sway (procedural, on top of the water) ------------------------------

    UPROPERTY(EditAnywhere, Category = "Boat|Sway", meta = (ClampMin = "0", UIMax = "100", Units = "Centimeters"))
    float BobAmplitude = 10.f;

    UPROPERTY(EditAnywhere, Category = "Boat|Sway", meta = (ClampMin = "0.1", Units = "Seconds"))
    float BobPeriod = 3.5f;

    UPROPERTY(EditAnywhere, Category = "Boat|Sway", meta = (ClampMin = "0", UIMax = "15", Units = "Degrees"))
    float PitchAmplitude = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Boat|Sway", meta = (ClampMin = "0.1", Units = "Seconds"))
    float PitchPeriod = 4.3f;

    UPROPERTY(EditAnywhere, Category = "Boat|Sway", meta = (ClampMin = "0", UIMax = "15", Units = "Degrees"))
    float RollAmplitude = 3.f;

    UPROPERTY(EditAnywhere, Category = "Boat|Sway", meta = (ClampMin = "0.1", Units = "Seconds"))
    float RollPeriod = 5.1f;

    /** Seed the sway phases from the actor name. Off = every boat bobs in step. */
    UPROPERTY(EditAnywhere, Category = "Boat|Sway")
    bool bRandomizePhase = true;

    // ---- Editor --------------------------------------------------------------

    /** Sail and bob in the editor viewport without PIE, for tuning. */
    UPROPERTY(EditAnywhere, Category = "Boat|Editor")
    bool bAnimateInEditor = false;

private:
    /** Advance by DeltaSeconds and place BoatPivot. bSnap skips smoothing (first frame, construction). */
    void UpdateBoat(float DeltaSeconds, bool bSnap);

    /** Surface Z at a world XY, or FallbackZ if there's no water there. */
    float SampleWaterZ(const FVector& WorldLocation, float FallbackZ) const;

    AWaterBody* ResolveWaterBody();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USplineComponent> Path;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> BoatPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> Hull;

    TWeakObjectPtr<AWaterBody> ResolvedWater;

    float Elapsed = 0.f;
    float PhaseBob = 0.f;
    float PhasePitch = 0.f;
    float PhaseRoll = 0.f;

    // Smoothed state
    float CurrentZ = 0.f;
    float CurrentPitch = 0.f;
    float CurrentRoll = 0.f;
};
