#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AtrophyTileMeshActor.generated.h"

class UAtrophyTileVolumeComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * AAtrophyTileMeshActor
 *
 * Tiles one static mesh through a box volume so large blockout shapes can be
 * laid down from a single cube mesh. Drop it in a level, pick a Mesh, and set
 * TileCount (instances along X, Y, Z). With the actor selected, the editor
 * draws a drag handle on each of the six faces of the volume; dragging one
 * outward/inward adds or removes a row of tiles along that axis (see
 * FAtrophyTileVolumeVisualizer in the AtrophyToolsEditor module).
 *
 * Layout:
 *   Tile (0,0,0) sits at the actor origin and the volume grows along local +X,
 *   +Y, +Z; tile (i,j,k) is at (i,j,k) * TileSize. The volume therefore spans
 *   [0, TileCount * TileSize] in local space (GetTileVolumeLocalBounds). Pulling
 *   a negative face handle moves the actor origin so the opposite face stays put.
 *
 *   Meshes are assumed to be TileSize-sized cubes (100x100x100 by default).
 *   With bAlignToMeshBounds the mesh's bounding-box minimum corner is placed on
 *   the tile corner, so a centred-pivot cube and a corner-pivot cube tile the
 *   same way.
 *
 * Instances are rebuilt from scratch in OnConstruction (placement, property
 * edits, handle drags, runtime spawn) and persist with the level, so nothing
 * runs at load. Instances added by hand to the Instances component are
 * discarded on the next rebuild.
 *
 * Future: multiple mesh slots (top / corner / edge meshes, or a set per slot).
 * That is why the tiling parameters live on the actor rather than on the ISM
 * component: the slots would each be another component under the same volume.
 */
UCLASS(BlueprintType, HideCategories = (Input, Replication, Networking, HLOD, Cooking, DataLayers))
class ATROPHYTOOLS_API AAtrophyTileMeshActor : public AActor
{
    GENERATED_BODY()

public:
    AAtrophyTileMeshActor();

    /** Per-axis upper bound on TileCount. Keeps a mistyped value from creating millions of instances. */
    static constexpr int32 MaxTilesPerAxis = 1024;

    /** The mesh instanced through the volume. Nothing is drawn while unset. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiling")
    TObjectPtr<UStaticMesh> Mesh;

    /** How many instances to lay side by side along each local axis. Whole tiles only, at least 1. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiling", meta = (ClampMin = "1", UIMin = "1", ClampMax = "1024"))
    FIntVector TileCount = FIntVector(1, 1, 1);

    /** Stride between neighbouring instances along each axis, in local units before actor scale. Match this to the mesh's dimensions. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiling", meta = (ClampMin = "1.0", UIMin = "1.0"))
    FVector TileSize = FVector(100.0, 100.0, 100.0);

    /**
     * When set, each instance is offset so the mesh's bounding-box minimum corner
     * lands on its tile's corner (works for centred-pivot cubes such as the engine
     * BasicShapes cube). When clear, the mesh pivot is placed on the tile corner.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiling")
    bool bAlignToMeshBounds = true;

    UFUNCTION(BlueprintCallable, Category = "Tiling")
    void SetMesh(UStaticMesh* InMesh);

    /** Sets the per-axis instance counts (clamped to [1, MaxTilesPerAxis]) and rebuilds. */
    UFUNCTION(BlueprintCallable, Category = "Tiling")
    void SetTileCount(FIntVector InTileCount);

    UFUNCTION(BlueprintCallable, Category = "Tiling")
    void SetTileSize(FVector InTileSize);

    /** Throws away every instance and re-lays the grid from the current Mesh / TileCount / TileSize. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Tiling")
    void RebuildInstances();

    /** Box the tiles occupy in actor-local space: origin to TileCount * TileSize. */
    UFUNCTION(BlueprintPure, Category = "Tiling")
    FBox GetTileVolumeLocalBounds() const;

    /** Number of instances currently in the Instances component. */
    UFUNCTION(BlueprintPure, Category = "Tiling")
    int32 GetInstanceCount() const;

    UAtrophyTileVolumeComponent* GetTileVolume() const { return TileVolume; }
    UInstancedStaticMeshComponent* GetInstances() const { return Instances; }

    // --- Pure helpers, exposed so they can be unit-tested without a world. ---

    /** Clamps each axis of a tile count into [1, MaxTilesPerAxis]. */
    static FIntVector ClampTileCount(const FIntVector& InTileCount);

    /** Local-space offset applied to every instance: -BoundingBox.Min when aligning to bounds, zero otherwise. */
    static FVector ComputeMeshOffset(const UStaticMesh* InMesh, bool bInAlignToMeshBounds);

    /** Appends one local-space transform per tile of a Count grid with the given stride, each shifted by Offset. */
    static void ComputeTileTransforms(const FIntVector& Count, const FVector& Size, const FVector& Offset, TArray<FTransform>& OutTransforms);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    /** Root. Anchor for the editor visualizer; see the class comment on UAtrophyTileVolumeComponent. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tiling", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAtrophyTileVolumeComponent> TileVolume;

    /** Holds the tiled instances. Its contents are owned by RebuildInstances. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tiling", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInstancedStaticMeshComponent> Instances;
};
