#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoViewPoint.generated.h"

class UCameraComponent;

/**
 * AOctoViewPoint
 *
 * A placeable FIXED camera. Drop one in the editor, pick a Role, and
 * AOctoGameMode uses it as the view target for the matching part of the flow.
 *
 * This is the counterpart to AOctoSpawnPoint: together they are the two editor
 * handles that decide WHERE each area of L_OctoOdyssey is. Both exist because
 * OctoOdyssey now packs a menu island and two courses into one level at
 * different X, and those X positions are a look-at-it call the user is expected
 * to re-tune by dragging — not something that should be buried in C++ or in a
 * Python table.
 *
 * The camera transform is the actor's own: identity rotation looks along +X,
 * which is the same side-on framing of the Y-Z play plane AOctoCamera uses (see
 * its class comment). Rotate the actor if a menu shot wants a different angle —
 * nothing here re-derives the rotation.
 *
 * Transitions to and from this camera go through
 * APlayerController::SetViewTargetWithBlend, so the smooth move between menu and
 * gameplay is the engine's own view blend rather than anything this actor does.
 */
UCLASS()
class PARTYBUTTONS_API AOctoViewPoint : public AActor
{
    GENERATED_BODY()

public:
    AOctoViewPoint();

    EOctoViewRole GetViewRole() const { return ViewRole; }

    UCameraComponent* GetCamera() const { return CameraComponent; }

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

    /**
     * What this camera is for. See EOctoViewRole.
     *
     * Named ViewRole, not Role: AActor already carries a replication Role, and a
     * second reflected property by that name on a subclass is a collision waiting
     * to happen.
     */
    UPROPERTY(EditAnywhere, Category = "Octo")
    EOctoViewRole ViewRole = EOctoViewRole::MainMenu;

    /** Vertical FOV. Independent of FOctoTuning::CameraFieldOfView — a fixed establishing shot wants its own framing. */
    UPROPERTY(EditAnywhere, Category = "Octo")
    float FieldOfView = 60.f;

private:
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UCameraComponent> CameraComponent;
};
