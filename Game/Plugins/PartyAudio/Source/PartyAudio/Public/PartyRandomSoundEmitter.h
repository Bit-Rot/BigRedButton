#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyRandomSoundEmitter.generated.h"

class UBoxComponent;
class UPartySoundEvent;

/**
 * APartyRandomSoundEmitter
 *
 * A soundscape scatterer: plays Event at a random point inside its box, at a
 * random interval. Gulls wheeling over an island, a distant buoy bell, gusts in
 * the rigging.
 *
 * This is how natural ambience is built instead of with one long recording:
 * a looped gull track is recognisable as a loop within half a minute, while
 * individual calls at random places and times never repeat the same way twice,
 * and each call is spatialised on its own, so the flock moves around the
 * listener.
 *
 * Starts on BeginPlay (bAutoStart) and can be toggled with SetEmitting.
 */
UCLASS()
class PARTYAUDIO_API APartyRandomSoundEmitter : public AActor
{
    GENERATED_BODY()

public:
    APartyRandomSoundEmitter();

    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void SetEmitting(bool bEmit);

    /** Set everything a placement script needs in one call (no struct wrappers required). */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void Configure(UPartySoundEvent* InEvent, FVector InBoxExtent, float InMinInterval, float InMaxInterval,
                   float InMinIntensity, float InMaxIntensity);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emitter")
    TObjectPtr<UPartySoundEvent> Event;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emitter", meta = (ClampMin = "0.05"))
    float MinIntervalSeconds = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emitter", meta = (ClampMin = "0.05"))
    float MaxIntervalSeconds = 12.f;

    /** Each play's intensity is uniform in [Min, Max] — near and far, calm and agitated. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emitter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinIntensity = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emitter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaxIntensity = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emitter")
    bool bAutoStart = true;

protected:
    virtual void BeginPlay() override;

private:
    void ScheduleNext();
    void Emit();

    UPROPERTY(VisibleAnywhere, Category = "Emitter")
    TObjectPtr<UBoxComponent> Area;

    FTimerHandle Timer;
    bool bEmitting = false;
};
