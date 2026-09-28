#include "PartyRandomSoundEmitter.h"
#include "PartyAudioSubsystem.h"
#include "PartySoundEvent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

APartyRandomSoundEmitter::APartyRandomSoundEmitter()
{
    PrimaryActorTick.bCanEverTick = false;

    Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
    Area->SetBoxExtent(FVector(500.f));
    Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Area->SetGenerateOverlapEvents(false);
    Area->ShapeColor = FColor(80, 200, 255);
    RootComponent = Area;
}

void APartyRandomSoundEmitter::Configure(UPartySoundEvent* InEvent, FVector InBoxExtent, float InMinInterval, float InMaxInterval,
                                         float InMinIntensity, float InMaxIntensity)
{
    Modify();
    Event = InEvent;
    Area->Modify();
    Area->SetBoxExtent(InBoxExtent);
    MinIntervalSeconds = FMath::Max(0.05f, InMinInterval);
    MaxIntervalSeconds = FMath::Max(MinIntervalSeconds, InMaxInterval);
    MinIntensity = FMath::Clamp(InMinIntensity, 0.f, 1.f);
    MaxIntensity = FMath::Clamp(InMaxIntensity, MinIntensity, 1.f);
}

void APartyRandomSoundEmitter::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoStart)
    {
        SetEmitting(true);
    }
}

void APartyRandomSoundEmitter::SetEmitting(bool bEmit)
{
    if (bEmit == bEmitting)
    {
        return;
    }
    bEmitting = bEmit;
    if (bEmitting)
    {
        ScheduleNext();
    }
    else if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(Timer);
    }
}

void APartyRandomSoundEmitter::ScheduleNext()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(Timer, this, &APartyRandomSoundEmitter::Emit,
                                          FMath::FRandRange(MinIntervalSeconds, MaxIntervalSeconds), false);
    }
}

void APartyRandomSoundEmitter::Emit()
{
    if (UPartyAudioSubsystem* Audio = UPartyAudioSubsystem::Get(this))
    {
        const FVector Point = UKismetMathLibrary::RandomPointInBoundingBox(Area->GetComponentLocation(), Area->GetScaledBoxExtent());
        // Each emitter keeps its own shuffle bag, so two gull boxes don't take
        // turns through one shared list.
        Audio->PlayEvent(Event, Point, FMath::FRandRange(MinIntensity, MaxIntensity),
                         FName(TEXT("Scatter"), static_cast<int32>(GetUniqueID())));
    }
    if (bEmitting)
    {
        ScheduleNext();
    }
}
