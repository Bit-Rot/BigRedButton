#include "OctoOdyssey/OctoGoalFlag.h"
#include "OctoOdyssey/OctoPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/HitResult.h"

AOctoGoalFlag::AOctoGoalFlag()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    Trigger->SetupAttachment(RootComponent);
    Trigger->SetBoxExtent(TriggerExtent);
    Trigger->SetCollisionProfileName(TEXT("Trigger")); // stock: QueryOnly, WorldDynamic, overlaps Pawn/PhysicsBody
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &AOctoGoalFlag::HandleBeginOverlap);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
    Pole->SetupAttachment(RootComponent);
    Pole->SetStaticMesh(CylinderMeshFinder.Object);
    Pole->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
    Pole->SetRelativeScale3D(FVector(0.1f, 0.1f, 3.f));
    Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pole->BodyInstance.bAutoWeld = false; // NoCollision alone doesn't stop autoweld — see AOctoPawn's class comment
    Pole->SetCastShadow(false);

    Flag = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flag"));
    Flag->SetupAttachment(RootComponent);
    Flag->SetStaticMesh(CubeMeshFinder.Object);
    Flag->SetRelativeLocation(FVector(0.f, 50.f, 260.f));
    Flag->SetRelativeScale3D(FVector(0.05f, 1.0f, 0.6f));
    Flag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Flag->BodyInstance.bAutoWeld = false;
    Flag->SetCastShadow(false);

    if (UMaterialInterface* BasicMaterial = BasicMaterialFinder.Object)
    {
        // RF_Transient: unlike PartyArena/PartyDuelPawn/PartyBullet (spawned
        // only at runtime, never saved), AOctoGoalFlag is placed directly
        // into L_GameC and saved as part of the level. A constructor-created
        // UMaterialInstanceDynamic left non-transient fails SavePackage with
        // "Illegal reference to private object" (its auto-generated
        // WITH_EDITORONLY_DATA companion object resolves back to the CDO's
        // own copy). Transient excludes it from serialization entirely — the
        // tint is just recreated fresh every time this constructor runs,
        // exactly like the runtime-only actors already do implicitly.
        if (UMaterialInstanceDynamic* PoleMID = UMaterialInstanceDynamic::Create(BasicMaterial, this))
        {
            PoleMID->SetFlags(RF_Transient);
            PoleMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.6f, 0.6f));
            Pole->SetMaterial(0, PoleMID);
        }
        if (UMaterialInstanceDynamic* FlagMID = UMaterialInstanceDynamic::Create(BasicMaterial, this))
        {
            FlagMID->SetFlags(RF_Transient);
            FlagMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.75f, 0.05f));
            Flag->SetMaterial(0, FlagMID);
        }
    }
}

void AOctoGoalFlag::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (bReached) { return; } // already fired — ignore further overlaps until the level reloads

    if (!Cast<AOctoPawn>(OtherActor)) { return; }

    bReached = true;
    OnReached.Broadcast();
}
