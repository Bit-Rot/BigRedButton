#include "OctoOdyssey/OctoTriggerVolume.h"
#include "OctoOdyssey/OctoPawn.h"
#include "Components/BoxComponent.h"
#include "Components/ShapeComponent.h"

AOctoTriggerVolume::AOctoTriggerVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    Trigger->SetupAttachment(RootComponent);
    Trigger->SetBoxExtent(TriggerExtent);
    Trigger->SetCollisionProfileName(TEXT("Trigger")); // stock: QueryOnly, WorldDynamic, overlaps Pawn/PhysicsBody
    Trigger->SetGenerateOverlapEvents(true);
}

void AOctoTriggerVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Only the built-in box. Extra shapes are the designer's, and their size is
    // the whole reason they were added — resizing them from one shared number
    // would defeat the point.
    if (Trigger)
    {
        Trigger->SetBoxExtent(TriggerExtent);
    }
}

void AOctoTriggerVolume::BeginPlay()
{
    Super::BeginPlay();

    if (bConfigureExtraColliders)
    {
        // Every shape, not just Trigger: instance-added components do not exist
        // when the constructor runs, so this is the first moment they can be
        // configured. Without it a box dropped in the Details panel keeps its
        // CDO's (blocking) profile and becomes a wall instead of a trigger —
        // see the class comment.
        TInlineComponentArray<UShapeComponent*> Shapes;
        GetComponents(Shapes);

        for (UShapeComponent* Shape : Shapes)
        {
            Shape->SetCollisionProfileName(TEXT("Trigger"));
            Shape->SetGenerateOverlapEvents(true);
        }
    }

    // Actor-level, not per-component. A component-level binding would have to be
    // repeated for every shape and would miss any added after this point; the
    // actor delegate covers all of them with one bind. The cost is that it can
    // fire once per overlapping shape, which every subclass is required to
    // tolerate (class comment).
    OnActorBeginOverlap.AddDynamic(this, &AOctoTriggerVolume::HandleActorBeginOverlap);
}

void AOctoTriggerVolume::HandleActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    if (AOctoPawn* Octo = Cast<AOctoPawn>(OtherActor))
    {
        NotifyOctoTouched(Octo);
    }
}
