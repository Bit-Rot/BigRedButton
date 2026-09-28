#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "PartyLoopAudioComponent.generated.h"

class UAudioComponent;
class UPartySoundEvent;

/**
 * UPartyLoopAudioComponent
 *
 * A continuous sound whose loudness, pitch and brightness follow one number —
 * the drive, 0..1 — set every frame by gameplay: rolling (drive = surface
 * speed), air rush (drive = airspeed), an engine (drive = throttle).
 *
 * The event's intensity shaping is evaluated at the smoothed drive, so
 * everything about how the loop responds is authored on the UPartySoundEvent;
 * this component only decides WHEN it responds:
 *   - AttackSeconds / ReleaseSeconds smooth the drive, rising fast and falling
 *     slowly so a roll over a bump doesn't stutter;
 *   - the voice starts when the smoothed drive crosses StartThreshold, picking a
 *     fresh variant (and a random start offset if the event asks for one);
 *   - the voice stops once the drive has decayed to silence, so a loop that is
 *     not being heard costs no voice at all.
 *
 * Attach it where the sound comes from; the voice is attached to it.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class PARTYAUDIO_API UPartyLoopAudioComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UPartyLoopAudioComponent();

    /** The target drive, 0..1. Call every frame (or whenever it changes). */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void SetDrive(float NewDrive);

    /** The smoothed drive the voice is following right now. */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    float GetSmoothedDrive() const { return Smoothed; }

    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    bool IsVoicePlaying() const;

    /** Drop the drive to 0 and cut the voice immediately (respawn, pause, teleport). */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void StopNow();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop")
    TObjectPtr<UPartySoundEvent> Event;

    /** Time constant for the drive rising. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop", meta = (ClampMin = "0.0"))
    float AttackSeconds = 0.05f;

    /** Time constant for the drive falling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop", meta = (ClampMin = "0.0"))
    float ReleaseSeconds = 0.25f;

    /** Smoothed drive above which the voice starts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StartThreshold = 0.03f;

    /** Smoothed drive below which a falling voice is stopped. Keep under StartThreshold (hysteresis). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StopThreshold = 0.01f;

    /**
     * Overall level for this emitter, on top of the event's curve (e.g. a live
     * SFX volume tunable). Separately, below StartThreshold the voice is faded
     * toward silence so starting and stopping it is inaudible.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loop", meta = (ClampMin = "0.0"))
    float VolumeMultiplier = 1.f;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StartVoice();
    void ApplyShaping();

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> Voice;

    float Target   = 0.f;
    float Smoothed = 0.f;

    /** Per-play jitter, fixed for the life of one voice so the loop doesn't wobble. */
    float JitterGain  = 1.f;
    float JitterPitch = 1.f;
};
