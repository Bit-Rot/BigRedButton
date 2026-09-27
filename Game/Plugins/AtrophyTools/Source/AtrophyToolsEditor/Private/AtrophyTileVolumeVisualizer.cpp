#include "AtrophyTileVolumeVisualizer.h"
#include "AtrophyTileMeshActor.h"
#include "AtrophyTileVolumeComponent.h"
#include "CanvasTypes.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "ScopedTransaction.h"
#include "UnrealWidgetFwd.h"

#define LOCTEXT_NAMESPACE "AtrophyTileVolumeVisualizer"

IMPLEMENT_HIT_PROXY(HAtrophyTileFaceProxy, HComponentVisProxy);

namespace AtrophyTileVolumeVisualizerLocals
{
    constexpr float HandlePointSize = 12.0f;
    constexpr float ArrowHeadSize = 6.0f;
    constexpr float ArrowThickness = 2.0f;
    /** Arrow length is a fraction of camera distance so handles stay usable on huge and tiny volumes alike. */
    constexpr double ArrowLengthPerUnitDistance = 0.05;
    constexpr float MinArrowLength = 40.0f;
    constexpr float MaxArrowLength = 500.0f;
    constexpr float OrthoArrowLength = 100.0f;

    const FLinearColor OutlineColor(1.0f, 0.85f, 0.2f);
    const FLinearColor SelectedHandleColor = FLinearColor::White;

    EAxis::Type ToEAxis(int32 AxisIndex)
    {
        switch (AxisIndex)
        {
        case 0:  return EAxis::X;
        case 1:  return EAxis::Y;
        default: return EAxis::Z;
        }
    }
}

FLinearColor FAtrophyTileVolumeVisualizer::GetAxisColor(int32 AxisIndex)
{
    switch (AxisIndex)
    {
    case 0:  return FLinearColor(1.0f, 0.1f, 0.1f);
    case 1:  return FLinearColor(0.1f, 1.0f, 0.1f);
    default: return FLinearColor(0.2f, 0.4f, 1.0f);
    }
}

FVector FAtrophyTileVolumeVisualizer::GetFaceCenterLocal(const FBox& LocalBounds, int32 AxisIndex, bool bNegativeFace)
{
    FVector Center = LocalBounds.GetCenter();
    Center[AxisIndex] = bNegativeFace ? LocalBounds.Min[AxisIndex] : LocalBounds.Max[AxisIndex];
    return Center;
}

AAtrophyTileMeshActor* FAtrophyTileVolumeVisualizer::GetEditedActor() const
{
    return SelectedAxisIndex != INDEX_NONE ? EditedActor.Get() : nullptr;
}

UActorComponent* FAtrophyTileVolumeVisualizer::GetEditedComponent() const
{
    const AAtrophyTileMeshActor* Actor = GetEditedActor();
    return Actor ? Actor->GetTileVolume() : nullptr;
}

void FAtrophyTileVolumeVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    using namespace AtrophyTileVolumeVisualizerLocals;

    const USceneComponent* Volume = Cast<UAtrophyTileVolumeComponent>(Component);
    const AAtrophyTileMeshActor* Actor = Volume ? Cast<AAtrophyTileMeshActor>(Volume->GetOwner()) : nullptr;
    if (!Actor)
    {
        return;
    }

    const FTransform VolumeToWorld = Volume->GetComponentTransform();
    const FBox LocalBounds = Actor->GetTileVolumeLocalBounds();

    DrawWireBox(PDI, VolumeToWorld.ToMatrixWithScale(), LocalBounds, OutlineColor, SDPG_World, 1.0f);

    const bool bThisActorActive = (GetEditedActor() == Actor);

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const bool bNegativeFace = (Side == 1);
            const FVector WorldCenter = VolumeToWorld.TransformPosition(GetFaceCenterLocal(LocalBounds, AxisIndex, bNegativeFace));

            // GetScaledAxis (not GetUnitAxis) so a negatively scaled actor still points its handles outward.
            const FVector Outward = VolumeToWorld.GetScaledAxis(ToEAxis(AxisIndex)).GetSafeNormal() * (bNegativeFace ? -1.0 : 1.0);

            float ArrowLength = OrthoArrowLength;
            if (View->IsPerspectiveProjection())
            {
                const double Distance = FVector::Distance(View->ViewMatrices.GetViewOrigin(), WorldCenter);
                ArrowLength = FMath::Clamp(static_cast<float>(Distance * ArrowLengthPerUnitDistance), MinArrowLength, MaxArrowLength);
            }

            const bool bSelected = bThisActorActive && SelectedAxisIndex == AxisIndex && bSelectedNegativeFace == bNegativeFace;
            const FLinearColor Color = bSelected ? SelectedHandleColor : GetAxisColor(AxisIndex);

            PDI->SetHitProxy(new HAtrophyTileFaceProxy(Component, AxisIndex, bNegativeFace));
            const FMatrix ArrowToWorld = FRotationMatrix::MakeFromX(Outward) * FTranslationMatrix(WorldCenter);
            DrawDirectionalArrow(PDI, ArrowToWorld, Color, ArrowLength, ArrowHeadSize, SDPG_Foreground, ArrowThickness);
            PDI->DrawPoint(WorldCenter, Color, HandlePointSize, SDPG_Foreground);
            PDI->SetHitProxy(nullptr);
        }
    }
}

void FAtrophyTileVolumeVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
    const USceneComponent* Volume = Cast<UAtrophyTileVolumeComponent>(Component);
    const AAtrophyTileMeshActor* Actor = Volume ? Cast<AAtrophyTileMeshActor>(Volume->GetOwner()) : nullptr;
    if (!Actor || !GEngine)
    {
        return;
    }

    const FBox LocalBounds = Actor->GetTileVolumeLocalBounds();
    FVector TopCenter = LocalBounds.GetCenter();
    TopCenter.Z = LocalBounds.Max.Z;

    FVector2D Pixel;
    if (View->WorldToPixel(Volume->GetComponentTransform().TransformPosition(TopCenter), Pixel))
    {
        const FIntVector& Count = Actor->TileCount;
        const FString Label = FString::Printf(TEXT("%d x %d x %d"), Count.X, Count.Y, Count.Z);
        Canvas->DrawShadowedString(Pixel.X + 8.0, Pixel.Y - 16.0, Label, GEngine->GetSmallFont(), FLinearColor::White);
    }
}

bool FAtrophyTileVolumeVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
    if (!VisProxy || !VisProxy->IsA(HAtrophyTileFaceProxy::StaticGetType()) || !VisProxy->Component.IsValid())
    {
        return false;
    }

    AAtrophyTileMeshActor* Actor = Cast<AAtrophyTileMeshActor>(VisProxy->Component->GetOwner());
    if (!Actor)
    {
        return false;
    }

    const HAtrophyTileFaceProxy* FaceProxy = static_cast<const HAtrophyTileFaceProxy*>(VisProxy);
    EditedActor = Actor;
    SelectedAxisIndex = FaceProxy->AxisIndex;
    bSelectedNegativeFace = FaceProxy->bNegativeFace;
    DragAccumulator = 0.0;

    // Only translation makes sense for a face handle.
    InViewportClient->SetWidgetMode(UE::Widget::WM_Translate);
    return true;
}

void FAtrophyTileVolumeVisualizer::EndEditing()
{
    EditedActor.Reset();
    SelectedAxisIndex = INDEX_NONE;
    bSelectedNegativeFace = false;
    DragAccumulator = 0.0;
}

bool FAtrophyTileVolumeVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
    const AAtrophyTileMeshActor* Actor = GetEditedActor();
    if (!Actor)
    {
        return false;
    }

    const FVector LocalCenter = GetFaceCenterLocal(Actor->GetTileVolumeLocalBounds(), SelectedAxisIndex, bSelectedNegativeFace);
    OutLocation = Actor->GetTileVolume()->GetComponentTransform().TransformPosition(LocalCenter);
    return true;
}

bool FAtrophyTileVolumeVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
    const AAtrophyTileMeshActor* Actor = GetEditedActor();
    if (!Actor)
    {
        return false;
    }

    // Line the widget's arrows up with the volume's local axes so the relevant one points straight out of the face.
    OutMatrix = FRotationMatrix::Make(Actor->GetTileVolume()->GetComponentQuat());
    return true;
}

bool FAtrophyTileVolumeVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
    using namespace AtrophyTileVolumeVisualizerLocals;

    AAtrophyTileMeshActor* Actor = GetEditedActor();
    if (!Actor)
    {
        return false;
    }

    // From here on the drag belongs to the handle: return true so the editor does not also move the actor.
    if (DeltaTranslate.IsNearlyZero())
    {
        return true;
    }

    const FTransform VolumeToWorld = Actor->GetTileVolume()->GetComponentTransform();
    const FVector ScaledAxis = VolumeToWorld.GetScaledAxis(ToEAxis(SelectedAxisIndex));
    const double WorldTileLength = ScaledAxis.Size() * Actor->TileSize[SelectedAxisIndex];
    if (WorldTileLength <= UE_KINDA_SMALL_NUMBER)
    {
        return true;
    }

    // Positive = dragging away from the volume, on either face.
    const FVector AxisDir = ScaledAxis / ScaledAxis.Size();
    const double OutwardSign = bSelectedNegativeFace ? -1.0 : 1.0;
    DragAccumulator += FVector::DotProduct(DeltaTranslate, AxisDir) * OutwardSign;

    const int32 WantedSteps = FMath::TruncToInt32(DragAccumulator / WorldTileLength);
    if (WantedSteps == 0)
    {
        return true;
    }

    const int32 Current = Actor->TileCount[SelectedAxisIndex];
    const int32 Applied = FMath::Clamp(Current + WantedSteps, 1, AAtrophyTileMeshActor::MaxTilesPerAxis) - Current;

    if (Applied == WantedSteps)
    {
        DragAccumulator -= WantedSteps * WorldTileLength;
    }
    else
    {
        // Hit the min/max: drop the surplus so the handle responds the moment the drag reverses.
        DragAccumulator = 0.0;
    }

    if (Applied != 0)
    {
        ApplyTileStep(*Actor, Applied, AxisDir, WorldTileLength);
    }
    return true;
}

void FAtrophyTileVolumeVisualizer::ApplyTileStep(AAtrophyTileMeshActor& Actor, int32 Steps, const FVector& WorldAxisDir, double WorldTileLength) const
{
    // Nests inside the viewport's "Move Elements" transaction while dragging, and stands alone otherwise.
    const FScopedTransaction Transaction(LOCTEXT("ResizeTileVolume", "Resize Tile Volume"));
    Actor.Modify();

    if (bSelectedNegativeFace)
    {
        // Grow/shrink towards -axis: move the origin so the far face does not move.
        Actor.SetActorLocation(Actor.GetActorLocation() - WorldAxisDir * (Steps * WorldTileLength));
    }

    Actor.TileCount[SelectedAxisIndex] += Steps;

    // Goes through the normal edit path so the actor re-runs construction and the details panel refreshes.
    FProperty* TileCountProperty = FindFProperty<FProperty>(AAtrophyTileMeshActor::StaticClass(), GET_MEMBER_NAME_CHECKED(AAtrophyTileMeshActor, TileCount));
    FPropertyChangedEvent ChangedEvent(TileCountProperty, EPropertyChangeType::ValueSet);
    Actor.PostEditChangeProperty(ChangedEvent);
}

void FAtrophyTileVolumeVisualizer::TrackingStarted(FEditorViewportClient* InViewportClient)
{
    DragAccumulator = 0.0;
}

void FAtrophyTileVolumeVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
    DragAccumulator = 0.0;
}

#undef LOCTEXT_NAMESPACE
