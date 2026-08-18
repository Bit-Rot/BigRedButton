#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoTriggerVolume.generated.h"

class AOctoPawn;
class UBoxComponent;

/**
 * AOctoTriggerVolume
 *
 * Abstract base for the placeable overlap volumes in OctoOdyssey — kill volumes
 * and checkpoints. It exists for one reason that is worth stating plainly:
 *
 *   A PLACED INSTANCE MAY CARRY EXTRA COLLIDERS ADDED IN THE EDITOR.
 *
 * One box is not enough for a real hazard. A pit with a lip, an L-shaped
 * checkpoint gate, a lava pool that follows a ramp — all of those want two or
 * three boxes on one actor, added per-instance in the Details panel, not a new
 * actor per box (which would give a kill volume no identity and a checkpoint
 * three competing respawn points). So this class treats its own Trigger as the
 * DEFAULT shape rather than the only one: BeginPlay walks every UShapeComponent
 * on the actor, instance-added ones included, and configures each as a trigger.
 * Overlap detection is bound at the ACTOR level (AActor::OnActorBeginOverlap)
 * so a shape that did not exist at construction time still reports.
 *
 * That per-instance configuration is what makes "just add another box" work
 * with no code and no Blueprint. A shape dropped in the editor defaults to
 * whatever profile the component CDO carries — usually a BLOCKING one, which on
 * a kill volume would silently turn the hazard into a wall the octopus stands
 * on. bConfigureExtraColliders (on by default) is what stops that, and turning
 * it off is the escape hatch for an instance that genuinely wants a shape doing
 * something else.
 *
 * Subclasses implement NotifyOctoTouched and nothing else. Note it can fire more
 * than once per octopus — once per overlapping shape, and again if the octopus
 * leaves and returns — so every subclass must be idempotent: AOctoKillVolume
 * leans on AOctoGameMode's pending-respawn guard, AOctoCheckpoint is
 * latest-wins by construction.
 *
 * Like AOctoSpawnPoint and AOctoGoalFlag, every instance is tagged with the
 * EOctoCourse it belongs to, because all three islands share one level (see
 * EOctoCourse) and a volume on the far island must not act on the run in
 * progress.
 */
UCLASS(Abstract)
class PARTYBUTTONS_API AOctoTriggerVolume : public AActor
{
    GENERATED_BODY()

public:
    AOctoTriggerVolume();

    EOctoCourse GetCourse() const { return Course; }

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    /**
     * An AOctoPawn began overlapping one of this actor's shapes.
     *
     * May fire several times for one octopus — see the class comment. Never
     * called with a null pawn.
     */
    virtual void NotifyOctoTouched(AOctoPawn* Octo) PURE_VIRTUAL(AOctoTriggerVolume::NotifyOctoTouched, );

    /**
     * Half-size of the built-in Trigger box, pushed into the component by
     * OnConstruction so it can be resized per instance in the editor.
     *
     * EditAnywhere, unlike AOctoGoalFlag::TriggerExtent (EditDefaultsOnly): a
     * goal flag is one shape that is always the same size, whereas the whole
     * point of a hazard is that it is the size of the hazard.
     *
     * The X default is generous because the play plane is only a plane to the
     * camera — SK_Okto's head sphere sits ~85cm out of it toward the viewer, so
     * a volume thin in X can be passed straight through by the part of the
     * octopus the player is actually looking at.
     */
    UPROPERTY(EditAnywhere, Category = "Octo")
    FVector TriggerExtent = FVector(400.f, 100.f, 200.f);

    /** Which course this volume belongs to. See EOctoCourse. */
    UPROPERTY(EditAnywhere, Category = "Octo")
    EOctoCourse Course = EOctoCourse::Normal;

    /**
     * Force every shape on this actor — including ones added per instance in the
     * editor — to the Trigger collision profile at BeginPlay.
     *
     * Leave this on unless you have deliberately set a shape up to do something
     * other than trigger. See the class comment for what turning it off costs.
     */
    UPROPERTY(EditAnywhere, Category = "Octo")
    bool bConfigureExtraColliders = true;

    /** The default shape. Extra shapes added per instance are peers of this one, not children of it. */
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UBoxComponent> Trigger;

private:
    UFUNCTION()
    void HandleActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
};
