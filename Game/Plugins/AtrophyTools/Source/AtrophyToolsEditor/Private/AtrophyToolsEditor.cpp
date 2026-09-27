#include "AtrophyToolsEditor.h"
#include "AtrophyTileVolumeComponent.h"
#include "AtrophyTileVolumeVisualizer.h"
#include "ComponentVisualizer.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"

void FAtrophyToolsEditorModule::StartupModule()
{
    // Loaded at PostEngineInit so GUnrealEd exists. Commandlets without an editor engine get no visualizers, which is fine.
    if (!GUnrealEd)
    {
        return;
    }

    const FName TileVolumeClassName = UAtrophyTileVolumeComponent::StaticClass()->GetFName();
    TSharedPtr<FComponentVisualizer> TileVolumeVisualizer = MakeShared<FAtrophyTileVolumeVisualizer>();
    GUnrealEd->RegisterComponentVisualizer(TileVolumeClassName, TileVolumeVisualizer);
    TileVolumeVisualizer->OnRegister();
    RegisteredVisualizerClassNames.Add(TileVolumeClassName);
}

void FAtrophyToolsEditorModule::ShutdownModule()
{
    if (GUnrealEd)
    {
        for (const FName& ClassName : RegisteredVisualizerClassNames)
        {
            GUnrealEd->UnregisterComponentVisualizer(ClassName);
        }
    }
    RegisteredVisualizerClassNames.Reset();
}

IMPLEMENT_MODULE(FAtrophyToolsEditorModule, AtrophyToolsEditor);
