#include "OctoOdyssey/OctoCheckpoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"

AOctoCheckpoint::AOctoCheckpoint()
{
    // A gate rather than a pool: thin along the course (Y), tall enough that an
    // octopus flying over the top still arms it. See AOctoKillVolume's
    // constructor for why this is pushed into the component here.
    TriggerExtent = FVector(400.f, 100.f, 500.f);
    Trigger->SetBoxExtent(TriggerExtent);

    // Green, and a different colour from AOctoSpawnPoint's amber, so a viewport
    // full of markers still reads at a glance: amber is where the course starts,
    // green is where it resumes.
    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    Arrow->SetupAttachment(RootComponent);
    Arrow->SetHiddenInGame(true);
    Arrow->ArrowSize = 2.f;
    Arrow->ArrowColor = FColor(40, 220, 90);
}

FVector AOctoCheckpoint::GetRespawnLocation() const
{
    // The ARROW's transform, not the actor's. They are the same until someone
    // drags the arrow off the box, and being able to do that is the reason the
    // arrow exists — see the class comment.
    return Arrow ? Arrow->GetComponentLocation() : GetActorLocation();
}

void AOctoCheckpoint::NotifyOctoTouched(AOctoPawn* Octo)
{
    // Fires again on every shape and every re-entry, which is harmless: the
    // GameMode's rule is latest-wins, so re-announcing the checkpoint that is
    // already current changes nothing.
    OnReached.Broadcast(this);
}
