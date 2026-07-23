#include "PartyGameModeBase.h"
#include "PartyButtons.h"
#include "PartyFlowHUD.h"
#include "PartyFlowRouter.h"
#include "PartySessionSubsystem.h"
#include "PartyInputController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APartyGameModeBase::APartyGameModeBase()
{
    PlayerControllerClass = APartyInputController::StaticClass();
    HUDClass              = APartyFlowHUD::StaticClass();
    DefaultPawnClass      = nullptr;
}

// ---- Lifecycle -------------------------------------------------------------

void APartyGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // Note: delegate binding is done in OnPostLogin where the PC is guaranteed to exist.
    // BeginPlay cannot safely call GetFirstPlayerController in all ordering scenarios.
}

void APartyGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    APartyInputController* PIC = Cast<APartyInputController>(NewPlayer);
    if (!PIC)
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("%s: OnPostLogin got a non-APartyInputController — delegates not bound."),
            *GetName());
        return;
    }

    BoundController = PIC;

    PIC->OnButtonPressed.AddUObject(this,   &APartyGameModeBase::HandlePlayerButtonDelegate);
    PIC->OnButtonReleased.AddUObject(this,  &APartyGameModeBase::HandlePlayerButtonReleasedDelegate);
    PIC->OnMainButtonTapped.AddUObject(this, &APartyGameModeBase::HandleMainTapDelegate);
    PIC->OnMainButtonHeld.AddUObject(this,   &APartyGameModeBase::HandleMainHoldDelegate);
    PIC->OnDevIncrement.AddUObject(this,     &APartyGameModeBase::HandleDevIncrementDelegate);
    PIC->OnDevDecrement.AddUObject(this,     &APartyGameModeBase::HandleDevDecrementDelegate);

    UE_LOG(LogPartyButtons, Log,
        TEXT("%s: bound player + main-button + dev-key delegates to %s."),
        *GetName(), *PIC->GetName());
}

// ---- HUD-facing defaults ---------------------------------------------------

FString APartyGameModeBase::GetHudTitle() const
{
    return TEXT("PartyButtons");
}

bool APartyGameModeBase::IsTileJoined(int32 PlayerIndex) const
{
    const UPartySessionSubsystem* S = Session();
    return S ? S->IsPlayerRegistered(PlayerIndex) : false;
}

// ---- Session convenience ---------------------------------------------------

UPartySessionSubsystem* APartyGameModeBase::Session() const
{
    return UPartySessionSubsystem::Get(this);
}

// ---- Travel ----------------------------------------------------------------

void APartyGameModeBase::TravelToPhase(EPartyPhase Phase)
{
    // Write phase BEFORE travel so the destination HUD's first DrawHUD is correct.
    if (UPartySessionSubsystem* S = Session())
    {
        S->SetPhase(Phase);
    }

    const FPartyPhaseRoute Route = PartyFlow::GetRoute(Phase);
    const FString Options        = PartyFlow::BuildGameModeOption(Route.GameModeClassPath);
    const FName   TargetMap      = Route.MapName;

    UE_LOG(LogPartyButtons, Log,
        TEXT("%s: TravelToPhase %d -> %s%s"),
        *GetName(),
        static_cast<int32>(Phase),
        *TargetMap.ToString(),
        *Options);

    // Defer one tick to avoid calling OpenLevel from inside BeginPlay or a delegate
    // handler that fires during world initialization.
    GetWorldTimerManager().SetTimerForNextTick([this, TargetMap, Options]()
    {
        UGameplayStatics::OpenLevel(this, TargetMap, /*bAbsolute=*/true, Options);
    });
}

void APartyGameModeBase::TravelToGame(int32 RosterIndex)
{
    UPartySessionSubsystem* S = Session();
    if (!S)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("%s: TravelToGame called with no subsystem."), *GetName());
        return;
    }

    const TArray<FPartyGameInfo>& Roster = S->GetRoster();
    if (!Roster.IsValidIndex(RosterIndex))
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("%s: TravelToGame: RosterIndex %d out of range (%d entries)."),
            *GetName(), RosterIndex, Roster.Num());
        return;
    }

    S->SetCurrentGame(RosterIndex);
    S->SetPhase(EPartyPhase::Minigame);

    const FPartyGameInfo& Info = Roster[RosterIndex];

    // Most roster entries share PartyMinigameGameMode (the Minigame phase route).
    // A game may instead set its own GameModeClassPath (e.g. slot 0 -> PartyArenaGameMode)
    // to run bespoke rules while still traveling to its own map.
    const FString GameModeClassPath = Info.GameModeClassPath.IsEmpty()
        ? PartyFlow::GetRoute(EPartyPhase::Minigame).GameModeClassPath
        : Info.GameModeClassPath;
    const FString Options   = PartyFlow::BuildGameModeOption(GameModeClassPath);
    const FName   TargetMap = Info.MapName;

    UE_LOG(LogPartyButtons, Log,
        TEXT("%s: TravelToGame %d (%s) -> %s%s"),
        *GetName(), RosterIndex, *Info.DisplayName, *TargetMap.ToString(), *Options);

    GetWorldTimerManager().SetTimerForNextTick([this, TargetMap, Options]()
    {
        UGameplayStatics::OpenLevel(this, TargetMap, /*bAbsolute=*/true, Options);
    });
}
