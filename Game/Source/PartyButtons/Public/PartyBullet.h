#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyBullet.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;
class APartyDuelPawn;
struct FHitResult;

/**
 * APartyBullet
 *
 * Tiny projectile fired by APartyDuelPawn in the Reflex Rumble minigame.
 * Bounces off arena walls via UProjectileMovementComponent's built-in bounce;
 * kills the APartyDuelPawn it overlaps — unless that pawn is mid-reflect, in
 * which case the pawn redirects this bullet instead of dying (see
 * APartyDuelPawn::NotifyHitByBullet).
 *
 * Collision layout (deliberately uses only stock engine channels/profiles —
 * no Config/DefaultEngine.ini changes needed):
 *   - Object type WorldDynamic.
 *   - Blocks WorldStatic (the arena walls) so ProjectileMovementComponent's
 *     HandleImpact fires and bounces the bullet.
 *   - Overlaps Pawn (the duel pawns' collision spheres) so contact is
 *     resolved here in gameplay code rather than by the physics engine
 *     stopping the bullet outright.
 */
UCLASS()
class PARTYBUTTONS_API APartyBullet : public AActor
{
    GENERATED_BODY()

public:
    APartyBullet();

    /** Launch the bullet from its current location along Direction (XY-planar) at Speed. */
    void Fire(const FVector& Direction, float Speed, APartyDuelPawn* Shooter);

    /** Called by APartyDuelPawn::NotifyHitByBullet when the pawn reflects instead of dying. */
    void Redirect(const FVector& NewVelocity, APartyDuelPawn* NewOwnerPawn);

    /** Current flight velocity — read by the reflecting pawn to compute the bounce. */
    FVector GetVelocity() const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category = "PartyBullet")
    float RadiusMeters = 0.08f; // bumped up from 1/10 player size — needs to read clearly at the top-down camera's distance

    UPROPERTY(EditDefaultsOnly, Category = "PartyBullet")
    float LifetimeSeconds = 8.f;

    /** Ignore the owning pawn's own hitbox for this long after spawn/redirect, so it can't instantly self-hit. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyBullet")
    float SpawnGraceSeconds = 0.05f;

private:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UPROPERTY(VisibleAnywhere, Category = "PartyBullet")
    TObjectPtr<USphereComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyBullet")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyBullet")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    /** The pawn that most recently fired/redirected this bullet — see SpawnGraceSeconds. */
    UPROPERTY()
    TWeakObjectPtr<APartyDuelPawn> OwningPawn;

    double GraceStartTime = 0.0;
};
