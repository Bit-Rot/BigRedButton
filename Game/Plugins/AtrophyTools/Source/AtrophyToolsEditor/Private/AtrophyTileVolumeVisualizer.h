#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class AAtrophyTileMeshActor;
class UAtrophyTileVolumeComponent;

/** Hit proxy for one of the six face handles of a tile volume. */
struct HAtrophyTileFaceProxy : public HComponentVisProxy
{
    DECLARE_HIT_PROXY();

    HAtrophyTileFaceProxy(const UActorComponent* InComponent, int32 InAxisIndex, bool bInNegativeFace)
        : HComponentVisProxy(InComponent, HPP_Wireframe)
        , AxisIndex(InAxisIndex)
        , bNegativeFace(bInNegativeFace)
    {
    }

    /** 0 = X, 1 = Y, 2 = Z. */
    int32 AxisIndex;
    /** True for the face at the local origin side of the axis, false for the far face. */
    bool bNegativeFace;
};

/**
 * FAtrophyTileVolumeVisualizer
 *
 * Draws, for a selected AAtrophyTileMeshActor, the outline of its tile volume
 * and an arrow handle in the middle of each of its six faces. Clicking a handle
 * moves the transform widget onto that face; dragging the widget along the
 * face's axis then adds or removes whole rows of tiles: every full TileSize of
 * outward drag is +1 on that axis, every full TileSize inward is -1. Dragging a
 * negative face also shifts the actor so the opposite face stays where it was.
 *
 * Registered against UAtrophyTileVolumeComponent (the actor's root) in
 * FAtrophyToolsEditorModule::StartupModule.
 */
class FAtrophyTileVolumeVisualizer : public FComponentVisualizer
{
public:
    // FComponentVisualizer
    virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
    virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
    virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
    virtual void EndEditing() override;
    virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
    virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
    virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
    virtual void TrackingStarted(FEditorViewportClient* InViewportClient) override;
    virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
    virtual UActorComponent* GetEditedComponent() const override;

    /** Handle colour per axis (X red, Y green, Z blue), matching the transform widget. */
    static FLinearColor GetAxisColor(int32 AxisIndex);

    /** Centre of a face of the local tile volume, in local space. */
    static FVector GetFaceCenterLocal(const FBox& LocalBounds, int32 AxisIndex, bool bNegativeFace);

private:
    /** The actor whose face handle was clicked, if any. */
    TWeakObjectPtr<AAtrophyTileMeshActor> EditedActor;

    /** Which face is being edited; INDEX_NONE when no handle is active. */
    int32 SelectedAxisIndex = INDEX_NONE;
    bool bSelectedNegativeFace = false;

    /** Outward drag distance (world units) not yet turned into a whole tile. Reset when tracking starts. */
    double DragAccumulator = 0.0;

    AAtrophyTileMeshActor* GetEditedActor() const;
    void ApplyTileStep(AAtrophyTileMeshActor& Actor, int32 Steps, const FVector& WorldAxisDir, double WorldTileLength) const;
};
