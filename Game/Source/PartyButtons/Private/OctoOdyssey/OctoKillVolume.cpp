#include "OctoOdyssey/OctoKillVolume.h"
#include "Components/BoxComponent.h"

AOctoKillVolume::AOctoKillVolume()
{
    // Wider in Y than the base default: a hazard almost always spans a stretch of
    // the course rather than a doorway in it. Resize per instance — the whole
    // point of TriggerExtent being EditAnywhere.
    //
    // Pushed into the component here as well as in OnConstruction, so the CDO
    // itself is right. A map-placed actor is restored from its SERIALIZED
    // component state at runtime and never re-runs OnConstruction, so a class
    // default that only ever reached the box via OnConstruction would be correct
    // in the editor and wrong in a cooked build.
    TriggerExtent = FVector(400.f, 300.f, 200.f);
    Trigger->SetBoxExtent(TriggerExtent);
}

void AOctoKillVolume::NotifyOctoTouched(AOctoPawn* Octo)
{
    // No local guard: repeats are AOctoGameMode's to collapse — see the class
    // comment for why this cannot latch.
    OnTouched.Broadcast(Course);
}
