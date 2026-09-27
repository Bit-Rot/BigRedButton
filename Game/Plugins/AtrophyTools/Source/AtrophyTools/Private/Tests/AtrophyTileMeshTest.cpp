#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AtrophyTileMeshActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace AtrophyTileMeshTestLocals
{
    /** Engine 100cm cube, pivot at its centre (bounds -50..50). */
    UStaticMesh* LoadCube()
    {
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    }

    /** A throwaway game world registered with the engine so actors can be spawned into it. */
    struct FScopedTestWorld
    {
        UWorld* World = nullptr;

        FScopedTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AtrophyTileMeshTest"));
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(World);
        }

        ~FScopedTestWorld()
        {
            if (World)
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
            }
        }
    };
}

// ---------------------------------------------------------------------------
// ComputesFullGrid
//
// A 2x3x4 grid yields exactly 24 transforms, one per (i,j,k) * TileSize +
// Offset, with no duplicates.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAtrophyTileMeshGridTest,
    "PartyButtons.Atrophy.TileMesh.ComputesFullGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAtrophyTileMeshGridTest::RunTest(const FString& Parameters)
{
    const FIntVector Count(2, 3, 4);
    const FVector Size(100.0, 200.0, 50.0);
    const FVector Offset(5.0, 6.0, 7.0);

    TArray<FTransform> Transforms;
    AAtrophyTileMeshActor::ComputeTileTransforms(Count, Size, Offset, Transforms);

    TestEqual(TEXT("One transform per tile"), Transforms.Num(), 24);

    for (int32 Z = 0; Z < Count.Z; ++Z)
    {
        for (int32 Y = 0; Y < Count.Y; ++Y)
        {
            for (int32 X = 0; X < Count.X; ++X)
            {
                const FVector Expected = FVector(X, Y, Z) * Size + Offset;
                const int32 Matches = Transforms.FilterByPredicate([&Expected](const FTransform& T)
                {
                    return T.GetLocation().Equals(Expected);
                }).Num();
                TestEqual(*FString::Printf(TEXT("Tile (%d,%d,%d) appears exactly once"), X, Y, Z), Matches, 1);
            }
        }
    }

    for (const FTransform& T : Transforms)
    {
        TestTrue(TEXT("Tiles are not rotated"), T.GetRotation().IsIdentity());
        TestTrue(TEXT("Tiles are not scaled"), T.GetScale3D().Equals(FVector::OneVector));
    }

    return true;
}

// ---------------------------------------------------------------------------
// ClampsTileCount
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAtrophyTileMeshClampTest,
    "PartyButtons.Atrophy.TileMesh.ClampsTileCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAtrophyTileMeshClampTest::RunTest(const FString& Parameters)
{
    const FIntVector Clamped = AAtrophyTileMeshActor::ClampTileCount(FIntVector(0, -3, 5));
    TestEqual(TEXT("Zero clamps to one"), Clamped.X, 1);
    TestEqual(TEXT("Negative clamps to one"), Clamped.Y, 1);
    TestEqual(TEXT("In-range value is kept"), Clamped.Z, 5);

    const FIntVector Huge = AAtrophyTileMeshActor::ClampTileCount(FIntVector(1000000, 1, 1));
    TestEqual(TEXT("Oversized value clamps to MaxTilesPerAxis"), Huge.X, AAtrophyTileMeshActor::MaxTilesPerAxis);
    return true;
}

// ---------------------------------------------------------------------------
// MeshOffsetAlignsBoundsMin
//
// The engine cube is centred on its pivot, so aligning its bounds min to the
// tile corner means shifting it by +50 on every axis.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAtrophyTileMeshOffsetTest,
    "PartyButtons.Atrophy.TileMesh.MeshOffsetAlignsBoundsMin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAtrophyTileMeshOffsetTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Cube = AtrophyTileMeshTestLocals::LoadCube();
    if (!TestNotNull(TEXT("Engine cube loads"), Cube))
    {
        return false;
    }

    const FVector Aligned = AAtrophyTileMeshActor::ComputeMeshOffset(Cube, true);
    TestTrue(TEXT("Aligned offset moves bounds min onto the origin"), Aligned.Equals(FVector(50.0, 50.0, 50.0), 0.5));

    TestTrue(TEXT("Unaligned offset is zero"), AAtrophyTileMeshActor::ComputeMeshOffset(Cube, false).IsZero());
    TestTrue(TEXT("Null mesh offset is zero"), AAtrophyTileMeshActor::ComputeMeshOffset(nullptr, true).IsZero());
    return true;
}

// ---------------------------------------------------------------------------
// SpawnLaysOutInstances
//
// Spawning the actor with a mesh and a 2x3x4 count produces 24 instances;
// SetTileCount rebuilds to the new total; clearing the mesh empties it; and
// the local bounds track TileCount * TileSize.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAtrophyTileMeshSpawnTest,
    "PartyButtons.Atrophy.TileMesh.SpawnLaysOutInstances",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAtrophyTileMeshSpawnTest::RunTest(const FString& Parameters)
{
    using namespace AtrophyTileMeshTestLocals;

    UStaticMesh* Cube = LoadCube();
    if (!TestNotNull(TEXT("Engine cube loads"), Cube))
    {
        return false;
    }

    FScopedTestWorld TestWorld;
    if (!TestNotNull(TEXT("Test world created"), TestWorld.World))
    {
        return false;
    }

    AAtrophyTileMeshActor* Actor = TestWorld.World->SpawnActorDeferred<AAtrophyTileMeshActor>(AAtrophyTileMeshActor::StaticClass(), FTransform::Identity);
    if (!TestNotNull(TEXT("Actor spawned"), Actor))
    {
        return false;
    }
    Actor->Mesh = Cube;
    Actor->TileCount = FIntVector(2, 3, 4);
    Actor->FinishSpawning(FTransform::Identity);

    TestEqual(TEXT("OnConstruction laid out every tile"), Actor->GetInstanceCount(), 24);
    TestTrue(TEXT("Local bounds span TileCount * TileSize"),
        Actor->GetTileVolumeLocalBounds().Max.Equals(FVector(200.0, 300.0, 400.0)));

    // First instance sits at the aligned corner offset, not at the raw pivot.
    FTransform First;
    if (TestTrue(TEXT("Instance 0 readable"), Actor->GetInstances()->GetInstanceTransform(0, First, false)))
    {
        TestTrue(TEXT("Instance 0 is bounds-aligned to the origin corner"), First.GetLocation().Equals(FVector(50.0, 50.0, 50.0), 0.5));
    }

    Actor->SetTileCount(FIntVector(5, 1, 1));
    TestEqual(TEXT("SetTileCount rebuilds to the new total"), Actor->GetInstanceCount(), 5);

    Actor->SetTileCount(FIntVector(0, 0, 0));
    TestEqual(TEXT("Zero count clamps to a single tile"), Actor->GetInstanceCount(), 1);

    Actor->SetMesh(nullptr);
    TestEqual(TEXT("No mesh means no instances"), Actor->GetInstanceCount(), 0);

    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
