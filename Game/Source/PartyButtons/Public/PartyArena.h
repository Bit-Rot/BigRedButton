#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Box2D.h"
#include "Math/RandomStream.h"
#include "PartyArena.generated.h"

class UStaticMeshComponent;
class UCameraComponent;
class UDirectionalLightComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * APartyArena
 *
 * Builds the Reflex Rumble playing field entirely at runtime in C++ — no
 * .uasset content, matching the project's "no editor GUI, no hand-authored
 * assets" philosophy (see AI/design/architecture.md): a square floor, four
 * boundary walls, a top-down camera framing the whole field, and a light so
 * the runtime-spawned geometry is visible with no lighting build. Spawned
 * once by APartyArenaGameMode::BeginPlay into the (otherwise empty) L_GameA map.
 *
 * All geometry comes from engine-provided basic shapes
 * (/Engine/BasicShapes/Plane, /Engine/BasicShapes/Cube) tinted via a
 * UMaterialInstanceDynamic on /Engine/BasicShapes/BasicShapeMaterial's
 * "Color" vector parameter (verified present on that asset).
 */
UCLASS()
class PARTYBUTTONS_API APartyArena : public AActor
{
    GENERATED_BODY()

public:
    APartyArena();

    /** Top-down camera framing the whole field. GameMode calls PC->SetViewTarget(this) using it. */
    UCameraComponent* GetCamera() const { return CameraComponent; }

    /** Full field size in Unreal units (cm), derived from FieldSizeMeters. */
    float GetFieldSizeUnits() const { return FieldSizeMeters * 100.f; }

    /**
     * Playable XY bounds (world space), inset so an actor with radius
     * ActorRadiusMeters doesn't clip through the walls.
     */
    FBox2D GetPlayBounds(float ActorRadiusMeters) const;

    /**
     * A random point within GetPlayBounds(ActorRadiusMeters), at floor height
     * plus that radius, with a random yaw. Callers wanting spacing between
     * multiple spawns should call this repeatedly and reject points that are
     * too close together (see APartyArenaGameMode::SpawnPawns).
     */
    FTransform GetRandomSpawnTransform(FRandomStream& Rng, float ActorRadiusMeters) const;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float FieldSizeMeters = 10.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float WallHeightMeters = 2.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float WallThicknessMeters = 0.5f;

    /**
     * Height of the top-down camera above the floor. Generous by default so
     * the square field stays fully in frame across common aspect ratios —
     * re-tune alongside CameraFieldOfView if you change FieldSizeMeters.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float CameraHeightMeters = 20.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyArena")
    float CameraFieldOfView = 60.f;

private:
    /** Shared setup for a wall mesh component: static mesh, transform, collision, tint. */
    void ConfigureWall(
        UStaticMeshComponent* Wall,
        UStaticMesh* Mesh,
        UMaterialInterface* BaseMaterial,
        const FVector& RelativeLocation,
        const FVector& RelativeScale);

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UStaticMeshComponent> Floor;

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UStaticMeshComponent> WallNorth;

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UStaticMeshComponent> WallSouth;

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UStaticMeshComponent> WallEast;

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UStaticMeshComponent> WallWest;

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UCameraComponent> CameraComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyArena")
    TObjectPtr<UDirectionalLightComponent> SunLight;
};
