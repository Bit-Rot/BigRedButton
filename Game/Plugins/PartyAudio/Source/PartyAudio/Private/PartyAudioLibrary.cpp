#include "PartyAudioLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

bool UPartyAudioLibrary::SetPhysicalMaterialSurfaceIndex(UPhysicalMaterial* Material, int32 SurfaceIndex)
{
    if (!Material || SurfaceIndex < 0 || SurfaceIndex >= SurfaceType_Max)
    {
        return false;
    }
    Material->Modify();
    Material->SurfaceType = static_cast<EPhysicalSurface>(SurfaceIndex);
    return true;
}

int32 UPartyAudioLibrary::GetPhysicalMaterialSurfaceIndex(const UPhysicalMaterial* Material)
{
    return Material ? static_cast<int32>(Material->SurfaceType.GetValue()) : 0;
}
