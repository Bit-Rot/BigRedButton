#include "PartyButtons.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogPartyButtons);

class FPartyButtonsModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
        UE_LOG(LogPartyButtons, Display, TEXT("PartyButtons primary game module loaded."));
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FPartyButtonsModule, PartyButtons, "PartyButtons");
