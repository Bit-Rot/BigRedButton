#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "PartyImpactAudioComponent.generated.h"

class UPrimitiveComponent;
class UPartySoundEvent;
struct FHitResult;

/**
 * UPartyImpactAudioComponent
 *
 * Physically driven impact sounds for a simulating body. Drop it on an actor,
 * point it at the body (defaults to the root primitive), fill in the events —
 * no gameplay code needed.
 *
 * Intensity is |NormalImpulse| / mass: the speed the contact actually took out
 * of the body. Unlike closing speed it ignores grazes — a body sliding fast
 * along a floor carries plenty of speed and almost no impulse — which is exactly
 * the difference the ear expects between a slide and a slam. It is mapped to
 * 0..1 between MinImpactSpeed (below: silent) and MaxImpactSpeed.
 *
 * Two layers per impact, the standard material-pair design:
 *   - SelfEvent — what this object IS (octopus flesh, a wooden crate);
 *   - SurfaceEvents[surface] — what it HIT, keyed by the other side's physical
 *     material surface type, falling back to DefaultSurfaceEvent.
 *
 * Resting contact reports hits every physics step. Two things keep that quiet:
 * MinImpactSpeed, and a re-arm rule — after a sound, the next one needs either
 * RetriggerSeconds to pass or a hit at least RetriggerIntensityJump harder.
 *
 * Also a contact sensor for free: every hit (loud or not) refreshes
 * GetSecondsSinceContact(), which a rolling loop can use as its "grounded" gate.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class PARTYAUDIO_API UPartyImpactAudioComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPartyImpactAudioComponent();

    /** Listen to Target's hits instead of the owner's root primitive. Safe before or after BeginPlay. */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void SetTarget(UPrimitiveComponent* Target);

    /**
     * Also listen to Extra's hits, as part of the same body. For shapes WELDED into
     * the target: a welded shape raises hits only if its own component asks for
     * them, so a body that mostly touches the world through welded parts (limbs,
     * bumpers) is silent without this. Safe before or after BeginPlay.
     */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void AddTarget(UPrimitiveComponent* Extra);

    /** Seconds since Target last reported any contact. Large when it never has. */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    float GetSecondsSinceContact() const;

    /** Change the speed range that maps to intensity 0..1 (e.g. from live tuning). */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    void SetImpactSpeedRange(float MinSpeed, float MaxSpeed);

    /** What this object is made of. Plays on every audible impact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    TObjectPtr<UPartySoundEvent> SelfEvent;

    /** What was hit, by the other side's surface type. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<UPartySoundEvent>> SurfaceEvents;

    /** Surface layer when the other side's surface has no entry in SurfaceEvents. Null = no surface layer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    TObjectPtr<UPartySoundEvent> DefaultSurfaceEvent;

    /** Speed change (cm/s) below which an impact is silent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
    float MinImpactSpeed = 60.f;

    /** Speed change (cm/s) that counts as the hardest hit (intensity 1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
    float MaxImpactSpeed = 1200.f;

    /** Mass for the impulse->speed conversion. 0 = read it from the body that was hit (its weld parent, if welded). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
    float MassOverrideKg = 0.f;

    /** After an impact sound, the next needs this much time to pass... */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
    float RetriggerSeconds = 0.1f;

    /** ...or to be at least this much harder (intensity units). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RetriggerIntensityJump = 0.3f;

    /** Overall level for this emitter (e.g. a live SFX volume tunable). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
    float VolumeMultiplier = 1.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                   FVector NormalImpulse, const FHitResult& Hit);

    void Bind();
    void Unbind();

    /** The other side's surface type: the hit's physical material, else the other component's. */
    EPhysicalSurface ResolveSurface(const FHitResult& Hit, const UPrimitiveComponent* OtherComponent) const;

    /** Every component whose hits are ours right now. */
    TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundTargets;

    TWeakObjectPtr<UPrimitiveComponent> RequestedTarget;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> ExtraTargets;

    FName SourceKey;

    double LastContactTime = -1.0e9;
    double LastSoundTime   = -1.0e9;
    float  LastSoundIntensity = 0.f;
};
