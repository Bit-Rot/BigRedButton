#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Editor-only half of AtrophyTools. Registers the component visualizers that
 * give the runtime actors their in-viewport handles.
 */
class FAtrophyToolsEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TArray<FName> RegisteredVisualizerClassNames;
};
