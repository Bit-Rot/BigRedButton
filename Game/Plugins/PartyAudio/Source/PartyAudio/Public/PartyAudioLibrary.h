#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PartyAudioLibrary.generated.h"

class UPhysicalMaterial;

/**
 * Small helpers for the scripts that build audio content (AI/build_audio_assets.py).
 */
UCLASS()
class PARTYAUDIO_API UPartyAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Set Material's surface type by index (1 = SurfaceType1, ...). Exists because
     * the numbered EPhysicalSurface values are UMETA(Hidden), which leaves them out
     * of the editor's Python enum — a script cannot otherwise name SurfaceType1.
     * Marks the material modified (undoable, dirties the package).
     */
    UFUNCTION(BlueprintCallable, Category = "PartyAudio")
    static bool SetPhysicalMaterialSurfaceIndex(UPhysicalMaterial* Material, int32 SurfaceIndex);

    /** The surface index Material reports (0 = Default). */
    UFUNCTION(BlueprintPure, Category = "PartyAudio")
    static int32 GetPhysicalMaterialSurfaceIndex(const UPhysicalMaterial* Material);
};
