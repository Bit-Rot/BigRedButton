#include "PartyArena.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APartyArena::APartyArena()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // Engine-provided basic shapes — referencing existing Engine content, not
    // authoring new .uasset files (see class comment / architecture.md).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    UStaticMesh* CubeMesh             = CubeMeshFinder.Object;
    UMaterialInterface* BasicMaterial = BasicMaterialFinder.Object;

    const float HalfField     = FieldSizeMeters * 100.f * 0.5f;
    const float WallThickness = WallThicknessMeters * 100.f;
    const float WallHeight    = WallHeightMeters * 100.f;

    // ---- Floor ---------------------------------------------------------
    Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
    Floor->SetupAttachment(RootComponent);
    Floor->SetStaticMesh(PlaneMeshFinder.Object);
    // /Engine/BasicShapes/Plane is a 100x100cm quad; scale factor == desired size in meters.
    Floor->SetRelativeScale3D(FVector(FieldSizeMeters, FieldSizeMeters, 1.f));
    Floor->SetCollisionProfileName(TEXT("NoCollision")); // visual only — bullets/pawns stay in the XY plane
    Floor->SetCastShadow(false);
    if (BasicMaterial)
    {
        if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, this))
        {
            MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.10f, 0.12f));
            Floor->SetMaterial(0, MID);
        }
    }

    // ---- Walls -----------------------------------------------------------
    // North/South span the full width plus two wall-thicknesses so they close
    // the corners; East/West span just the field height between them.
    WallNorth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallNorth"));
    WallNorth->SetupAttachment(RootComponent);
    WallSouth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallSouth"));
    WallSouth->SetupAttachment(RootComponent);
    WallEast = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallEast"));
    WallEast->SetupAttachment(RootComponent);
    WallWest = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallWest"));
    WallWest->SetupAttachment(RootComponent);

    const float NorthSouthSpan = (HalfField * 2.f) + (WallThickness * 2.f);
    const float EastWestSpan   = HalfField * 2.f;

    ConfigureWall(WallNorth, CubeMesh, BasicMaterial,
        FVector(0.f, HalfField + WallThickness * 0.5f, WallHeight * 0.5f),
        FVector(NorthSouthSpan / 100.f, WallThickness / 100.f, WallHeight / 100.f));

    ConfigureWall(WallSouth, CubeMesh, BasicMaterial,
        FVector(0.f, -(HalfField + WallThickness * 0.5f), WallHeight * 0.5f),
        FVector(NorthSouthSpan / 100.f, WallThickness / 100.f, WallHeight / 100.f));

    ConfigureWall(WallEast, CubeMesh, BasicMaterial,
        FVector(HalfField + WallThickness * 0.5f, 0.f, WallHeight * 0.5f),
        FVector(WallThickness / 100.f, EastWestSpan / 100.f, WallHeight / 100.f));

    ConfigureWall(WallWest, CubeMesh, BasicMaterial,
        FVector(-(HalfField + WallThickness * 0.5f), 0.f, WallHeight * 0.5f),
        FVector(WallThickness / 100.f, EastWestSpan / 100.f, WallHeight / 100.f));

    // ---- Camera ------------------------------------------------------------
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    CameraComponent->SetupAttachment(RootComponent);
    CameraComponent->SetRelativeLocation(FVector(0.f, 0.f, CameraHeightMeters * 100.f));
    CameraComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // straight down
    CameraComponent->SetFieldOfView(CameraFieldOfView);
    CameraComponent->bConstrainAspectRatio = false;

    // ---- Light ---------------------------------------------------------
    // Movable so the runtime-spawned field needs no lighting build.
    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(RootComponent);
    SunLight->SetRelativeRotation(FRotator(-60.f, -30.f, 0.f));
    SunLight->Intensity = 3.f;
    SunLight->SetMobility(EComponentMobility::Movable);
}

void APartyArena::ConfigureWall(
    UStaticMeshComponent* Wall,
    UStaticMesh* Mesh,
    UMaterialInterface* BaseMaterial,
    const FVector& RelativeLocation,
    const FVector& RelativeScale)
{
    if (!Wall) { return; }

    Wall->SetStaticMesh(Mesh);
    Wall->SetRelativeLocation(RelativeLocation);
    Wall->SetRelativeScale3D(RelativeScale);
    Wall->SetCollisionProfileName(TEXT("BlockAll")); // blocks bullets so ProjectileMovementComponent bounces them
    Wall->SetCastShadow(false);

    if (BaseMaterial)
    {
        if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this))
        {
            MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.10f, 0.10f));
            Wall->SetMaterial(0, MID);
        }
    }
}

FBox2D APartyArena::GetPlayBounds(float ActorRadiusMeters) const
{
    const float Half = FMath::Max(1.f, GetFieldSizeUnits() * 0.5f - ActorRadiusMeters * 100.f);
    const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
    return FBox2D(Center - FVector2D(Half, Half), Center + FVector2D(Half, Half));
}

FTransform APartyArena::GetRandomSpawnTransform(FRandomStream& Rng, float ActorRadiusMeters) const
{
    const FBox2D Bounds = GetPlayBounds(ActorRadiusMeters);
    const float X   = Rng.FRandRange(Bounds.Min.X, Bounds.Max.X);
    const float Y   = Rng.FRandRange(Bounds.Min.Y, Bounds.Max.Y);
    const float Z   = GetActorLocation().Z + ActorRadiusMeters * 100.f;
    const float Yaw = Rng.FRandRange(0.f, 360.f);
    return FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, Z));
}
