#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoGoalFlag.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;

/** Broadcast once when an AOctoPawn overlaps the goal trigger. */
DECLARE_MULTICAST_DELEGATE(FOnOctoGoalReached);

/**
 * AOctoGoalFlag
 *
 * Placeable win trigger for OctoOdyssey (L_GameC). A pole + flag visual (both
 * NoCollision — the box trigger is the only collision) sits over an overlap
 * volume; overlapping it with the AOctoPawn broadcasts OnReached, which
 * AOctoGameMode binds to ReloadCourse().
 */
UCLASS()
class PARTYBUTTONS_API AOctoGoalFlag : public AActor
{
    GENERATED_BODY()

public:
    AOctoGoalFlag();

    FOnOctoGoalReached OnReached;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FVector TriggerExtent = FVector(100.f, 100.f, 200.f);

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

    /** Guards against re-broadcasting every tick the octopus stays overlapping. */
    bool bReached = false;
};
