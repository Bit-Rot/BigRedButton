#include "PartyBullet.h"
#include "PartyButtons.h"
#include "PartyDuelPawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"

APartyBullet::APartyBullet()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    RootComponent = CollisionComponent;
    CollisionComponent->InitSphereRadius(RadiusMeters * 100.f);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);   // arena walls -> bounce
    CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);        // duel pawns -> our own handling
    CollisionComponent->SetGenerateOverlapEvents(true);
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &APartyBullet::HandleBeginOverlap);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    // Unlit + emissive engine material (MSM_Unlit, exposes a "Color" vector param that
    // feeds EmissiveColor) so the bullet reads as a bright neon glow instead of a dim
    // lit surface — much easier to track than /Engine/BasicShapes/BasicShapeMaterial.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeonMaterialFinder(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetStaticMesh(SphereMeshFinder.Object);
    // /Engine/BasicShapes/Sphere is ~100cm (1m) in diameter; scale to 2*RadiusMeters meters.
    MeshComponent->SetRelativeScale3D(FVector(RadiusMeters * 2.f));
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // CollisionComponent handles all collision
    MeshComponent->SetCastShadow(false);

    if (UMaterialInterface* BaseMat = NeonMaterialFinder.Object)
    {
        if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this))
        {
            // Values above 1.0 push the emissive into HDR bloom range for a neon look.
            MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(4.f, 0.08f, 0.08f));
            MeshComponent->SetMaterial(0, MID);
        }
    }

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->SetUpdatedComponent(CollisionComponent);
    ProjectileMovement->bShouldBounce             = true;
    ProjectileMovement->Bounciness                = 1.f;
    ProjectileMovement->Friction                  = 0.f;
    ProjectileMovement->bBounceAngleAffectsFriction = false;
    ProjectileMovement->ProjectileGravityScale     = 0.f; // stays in the XY plane
    ProjectileMovement->bRotationFollowsVelocity   = false;
    ProjectileMovement->bSweepCollision            = true;
    ProjectileMovement->MaxSpeed                   = 5000.f;
    ProjectileMovement->InitialSpeed               = 0.f; // Fire()/Redirect() set Velocity directly
}

void APartyBullet::BeginPlay()
{
    Super::BeginPlay();
    SetLifeSpan(LifetimeSeconds);
}

void APartyBullet::Fire(const FVector& Direction, float Speed, APartyDuelPawn* Shooter)
{
    OwningPawn     = Shooter;
    GraceStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    SetOwner(Shooter);

    ProjectileMovement->Velocity = Direction.GetSafeNormal2D() * Speed;
}

void APartyBullet::Redirect(const FVector& NewVelocity, APartyDuelPawn* NewOwnerPawn)
{
    OwningPawn     = NewOwnerPawn;
    GraceStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    SetOwner(NewOwnerPawn);

    ProjectileMovement->Velocity = NewVelocity;
}

FVector APartyBullet::GetVelocity() const
{
    return ProjectileMovement ? ProjectileMovement->Velocity : FVector::ZeroVector;
}

void APartyBullet::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    APartyDuelPawn* Pawn = Cast<APartyDuelPawn>(OtherActor);
    if (!Pawn) { return; }

    // Grace period: ignore the pawn that currently owns this bullet (the shooter,
    // or the pawn that most recently reflected it) so it can't instantly self-hit.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Pawn == OwningPawn.Get() && (Now - GraceStartTime) < SpawnGraceSeconds)
    {
        return;
    }

    const bool bReflected = Pawn->NotifyHitByBullet(this, SweepResult);
    if (!bReflected)
    {
        // The pawn died; this bullet is spent.
        Destroy();
    }
}
