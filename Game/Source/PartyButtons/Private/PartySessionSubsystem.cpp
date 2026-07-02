#include "PartySessionSubsystem.h"
#include "PartyButtons.h"
#include "Engine/GameInstance.h"

void UPartySessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // FPartySessionState's constructor already calls InitDefaultRoster() and sizes
    // the arrays, so no explicit initialization is required here. A log confirms startup.
    UE_LOG(LogPartyButtons, Log,
        TEXT("UPartySessionSubsystem: initialized (roster: %d games, %d games per session)."),
        State.GameRoster.Num(),
        State.GamesPerSession);
}

/*static*/ UPartySessionSubsystem* UPartySessionSubsystem::Get(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return nullptr;
    }
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
}
