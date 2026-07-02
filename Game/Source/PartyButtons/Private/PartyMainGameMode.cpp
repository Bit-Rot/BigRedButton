#include "PartyMainGameMode.h"
#include "PartyButtons.h"
#include "PartySessionSubsystem.h"

void APartyMainGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Reset any previous session so we start clean.
    if (UPartySessionSubsystem* S = Session())
    {
        S->ResetSession();
    }

    UE_LOG(LogPartyButtons, Log, TEXT("APartyMainGameMode: init complete — redirecting to MainMenu."));

    // TravelToPhase defers one tick, so it is safe to call from BeginPlay.
    TravelToPhase(EPartyPhase::Lobby);
}
