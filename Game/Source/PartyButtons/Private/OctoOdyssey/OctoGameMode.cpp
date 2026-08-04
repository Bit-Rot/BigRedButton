#include "OctoOdyssey/OctoGameMode.h"
#include "PartyButtons.h"
#include "OctoOdyssey/OctoPawn.h"
#include "OctoOdyssey/OctoCamera.h"
#include "OctoOdyssey/OctoGoalFlag.h"
#include "OctoOdyssey/OctoSpawnPoint.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "PartyFlowRouter.h"
#include "PartySessionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AOctoGameMode::AOctoGameMode()
{
    DefaultPawnClass = nullptr; // the octopus is spawned manually and never possessed — see class comment
    OctoPawnClass     = AOctoPawn::StaticClass();
    OctoCameraClass   = AOctoCamera::StaticClass();
}

FString AOctoGameMode::GetHudSubtitle() const
{
    return TEXT("8 buttons = 8 arms. Push off the world. Reach the flag.");
}

void AOctoGameMode::BeginPlay()
{
    Super::BeginPlay(); // resolves CurrentRosterIndex / CurrentGameName for the HUD

    SpawnOctopus();
    SpawnCamera();
    BindGoalFlag();
    MaybeSpawnFallbackLight();
}

void AOctoGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // Re-assert the view target here too — removes the BeginPlay-vs-PostLogin
    // ordering dependency APartyArenaGameMode/APartyArena have, for the cost
    // of one idempotent call.
    if (Camera && NewPlayer)
    {
        NewPlayer->SetViewTarget(Camera);
    }
}

void AOctoGameMode::SpawnOctopus()
{
    if (!GetWorld() || !OctoPawnClass) { return; }

    FVector SpawnLocation = FallbackSpawnLocation;
    for (TActorIterator<AOctoSpawnPoint> It(GetWorld()); It; ++It)
    {
        SpawnLocation = It->GetActorLocation();
        break;
    }

    // The DOF constraint (AOctoPawn::BeginPlay's SetConstraintMode) anchors
    // X wherever the body is at that moment — force X to the play plane so
    // an editor-nudged spawn point can't silently pin the whole game off-plane.
    SpawnLocation.X = PlayPlaneX;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    Octo = GetWorld()->SpawnActor<AOctoPawn>(OctoPawnClass, FTransform(SpawnLocation), Params);
    if (!Octo)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("AOctoGameMode: failed to spawn AOctoPawn."));
    }
}

void AOctoGameMode::SpawnCamera()
{
    if (!GetWorld() || !OctoCameraClass) { return; }

    Camera = GetWorld()->SpawnActor<AOctoCamera>(OctoCameraClass, FTransform::Identity);
    if (!Camera)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("AOctoGameMode: failed to spawn AOctoCamera."));
        return;
    }

    Camera->SetFollowTarget(Octo);

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->SetViewTarget(Camera);
    }
}

void AOctoGameMode::BindGoalFlag()
{
    if (!GetWorld()) { return; }

    for (TActorIterator<AOctoGoalFlag> It(GetWorld()); It; ++It)
    {
        It->OnReached.AddUObject(this, &AOctoGameMode::HandleGoalReached);
        return;
    }

    UE_LOG(LogPartyButtons, Warning,
        TEXT("AOctoGameMode: no AOctoGoalFlag found in the level — the course cannot be won."));
}

void AOctoGameMode::MaybeSpawnFallbackLight()
{
    if (!bSpawnFallbackLight || !GetWorld()) { return; }

    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
    {
        return; // a light already exists — nothing to do
    }

    const FTransform LightTransform(FRotator(-50.f, 200.f, 0.f), FVector::ZeroVector);
    if (ADirectionalLight* Light = GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), LightTransform))
    {
        if (UDirectionalLightComponent* Comp = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
        {
            Comp->SetMobility(EComponentMobility::Movable);
            Comp->Intensity = 4.f;
        }
        UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: no light found in the level — spawned a fallback."));
    }
}

void AOctoGameMode::OnPlayerButton(int32 PlayerIndex)
{
    if (!Octo || PlayerIndex < 0 || PlayerIndex >= OctoArm::NumArms) { return; }
    if (bRequireRegistration && !IsTileJoined(PlayerIndex)) { return; }
    Octo->NotifyArmPressed(PlayerIndex);
}

void AOctoGameMode::OnPlayerButtonReleased(int32 PlayerIndex)
{
    if (!Octo || PlayerIndex < 0 || PlayerIndex >= OctoArm::NumArms) { return; }
    if (bRequireRegistration && !IsTileJoined(PlayerIndex)) { return; }
    Octo->NotifyArmReleased(PlayerIndex);
}

void AOctoGameMode::HandleGoalReached()
{
    UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: goal reached — reloading the course."));
    ReloadCourse();
}

void AOctoGameMode::ReloadCourse()
{
    if (bReloading) { return; } // multiple overlaps in one frame must not stack up travel calls
    bReloading = true;

    // GetMapName() is PIE-prefixed ("UEDPIE_0_L_GameC") and unusable for
    // OpenLevel — read the clean short name APartyMinigameGameMode::BeginPlay
    // already resolved onto the roster instead of re-deriving it.
    FName TargetMap = FallbackMapName;
    if (const UPartySessionSubsystem* S = Session())
    {
        const TArray<FPartyGameInfo>& Roster = S->GetRoster();
        if (Roster.IsValidIndex(CurrentRosterIndex))
        {
            TargetMap = Roster[CurrentRosterIndex].MapName;
        }
    }

    // The ?game= option is mandatory — without it, reopening L_GameC falls
    // back to GlobalDefaultGameMode SILENTLY (see PartyFlowRouter.h). The
    // reflected path has no 'A' prefix: OctoGameMode, not AOctoGameMode.
    const FString Options = PartyFlow::BuildGameModeOption(TEXT("/Script/PartyButtons.OctoGameMode"));

    UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: reloading %s%s"), *TargetMap.ToString(), *Options);

    // Deferred one tick, matching every other travel call in this codebase —
    // never call OpenLevel synchronously from inside a delegate handler that
    // fires during the current tick's physics/overlap processing.
    GetWorldTimerManager().SetTimerForNextTick([this, TargetMap, Options]()
    {
        UGameplayStatics::OpenLevel(this, TargetMap, /*bAbsolute=*/true, Options);
    });
}
