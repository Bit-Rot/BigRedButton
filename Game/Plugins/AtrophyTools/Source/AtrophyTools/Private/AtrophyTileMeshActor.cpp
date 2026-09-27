#include "AtrophyTileMeshActor.h"
#include "AtrophyTileVolumeComponent.h"
#include "AtrophyTools.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AAtrophyTileMeshActor::AAtrophyTileMeshActor()
{
    PrimaryActorTick.bCanEverTick = false;

    TileVolume = CreateDefaultSubobject<UAtrophyTileVolumeComponent>(TEXT("TileVolume"));
    SetRootComponent(TileVolume);

    Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
    Instances->SetupAttachment(TileVolume);
    Instances->SetMobility(EComponentMobility::Static);
}

void AAtrophyTileMeshActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildInstances();
}

void AAtrophyTileMeshActor::SetMesh(UStaticMesh* InMesh)
{
    Mesh = InMesh;
    RebuildInstances();
}

void AAtrophyTileMeshActor::SetTileCount(FIntVector InTileCount)
{
    TileCount = ClampTileCount(InTileCount);
    RebuildInstances();
}

void AAtrophyTileMeshActor::SetTileSize(FVector InTileSize)
{
    TileSize = InTileSize.ComponentMax(FVector(1.0));
    RebuildInstances();
}

void AAtrophyTileMeshActor::RebuildInstances()
{
    if (!Instances)
    {
        return;
    }

    // Keep the stored values legal even if they were set without the setters.
    TileCount = ClampTileCount(TileCount);
    TileSize = TileSize.ComponentMax(FVector(1.0));

    Instances->SetStaticMesh(Mesh);
    Instances->ClearInstances();

    if (!Mesh)
    {
        return;
    }

    TArray<FTransform> Transforms;
    ComputeTileTransforms(TileCount, TileSize, ComputeMeshOffset(Mesh, bAlignToMeshBounds), Transforms);
    Instances->AddInstances(Transforms, /*bShouldReturnIndices*/ false);

    UE_LOG(LogAtrophyTools, Verbose, TEXT("%s: laid %d instances of %s (%d x %d x %d)"),
        *GetName(), Transforms.Num(), *Mesh->GetName(), TileCount.X, TileCount.Y, TileCount.Z);
}

FBox AAtrophyTileMeshActor::GetTileVolumeLocalBounds() const
{
    const FIntVector Count = ClampTileCount(TileCount);
    return FBox(FVector::ZeroVector, FVector(Count.X, Count.Y, Count.Z) * TileSize);
}

int32 AAtrophyTileMeshActor::GetInstanceCount() const
{
    return Instances ? Instances->GetInstanceCount() : 0;
}

FIntVector AAtrophyTileMeshActor::ClampTileCount(const FIntVector& InTileCount)
{
    return FIntVector(
        FMath::Clamp(InTileCount.X, 1, MaxTilesPerAxis),
        FMath::Clamp(InTileCount.Y, 1, MaxTilesPerAxis),
        FMath::Clamp(InTileCount.Z, 1, MaxTilesPerAxis));
}

FVector AAtrophyTileMeshActor::ComputeMeshOffset(const UStaticMesh* InMesh, bool bInAlignToMeshBounds)
{
    if (!InMesh || !bInAlignToMeshBounds)
    {
        return FVector::ZeroVector;
    }
    return -InMesh->GetBoundingBox().Min;
}

void AAtrophyTileMeshActor::ComputeTileTransforms(const FIntVector& Count, const FVector& Size, const FVector& Offset, TArray<FTransform>& OutTransforms)
{
    OutTransforms.Reserve(OutTransforms.Num() + Count.X * Count.Y * Count.Z);

    for (int32 Z = 0; Z < Count.Z; ++Z)
    {
        for (int32 Y = 0; Y < Count.Y; ++Y)
        {
            for (int32 X = 0; X < Count.X; ++X)
            {
                OutTransforms.Emplace(FVector(X, Y, Z) * Size + Offset);
            }
        }
    }
}
