#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoGoalFlag.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;

/** Broadcast once when an AOctoPawn overlaps the goal trigger. Carries the course it ends. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOctoGoalReached, EOctoCourse /*Course*/);

/**
 * AOctoGoalFlag
 *
 * Placeable win trigger for OctoOdyssey. A pole + flag visual (both NoCollision —
 * the box trigger is the only collision) sits over an overlap volume; overlapping
 * it with the AOctoPawn broadcasts OnReached, which AOctoGameMode binds to its
 * score-entry flow.
 *
 * One per course, tagged with Course, because both courses share a level: the
 * broadcast carries which one finished so a stray overlap on the far island can
 * be ignored rather than ending the run in progress.
 */
UCLASS()
class PARTYBUTTONS_API AOctoGoalFlag : public AActor
{
    GENERATED_BODY()

public:
    AOctoGoalFlag();

    FOnOctoGoalReached OnReached;

    EOctoCourse GetCourse() const { return Course; }

    /**
     * Re-arm the one-shot so this flag can be reached again.
     *
     * Load-bearing now in a way it wasn't before: the flag used to fire once and
     * stay latched because reaching it RELOADED the map, which built a fresh
     * actor. OctoOdyssey never travels any more, so the same actor survives every
     * run — without this call (AOctoGameMode::EnterCourse) the second run of a
     * session could never be completed.
     */
    void ResetTrigger() { bReached = false; }

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FVector TriggerExtent = FVector(100.f, 100.f, 200.f);

    /** Which course reaching this flag completes. See EOctoCourse. */
    UPROPERTY(EditAnywhere, Category = "Octo")
    EOctoCourse Course = EOctoCourse::Normal;

private:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UBoxComponent> Trigger;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UStaticMeshComponent> Pole;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UStaticMeshComponent> Flag;

    /** Guards against re-broadcasting every tick the octopus stays overlapping. Cleared by ResetTrigger. */
    bool bReached = false;
};
