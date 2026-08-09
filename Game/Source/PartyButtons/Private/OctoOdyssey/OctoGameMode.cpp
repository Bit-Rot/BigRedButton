#include "OctoOdyssey/OctoGameMode.h"
#include "PartyButtons.h"
#include "OctoOdyssey/OctoPawn.h"
#include "OctoOdyssey/OctoCamera.h"
#include "OctoOdyssey/OctoGoalFlag.h"
#include "OctoOdyssey/OctoSpawnPoint.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "OctoOdyssey/OctoTuningSubsystem.h"
#include "PartyFlowRouter.h"
#include "PartySessionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
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
    Super::BeginPlay(); // resolves CurrentRosterIndex / CurrentGameName, resets the intro overlay

    // Before SpawnOctopus: the pawn pulls its own copy of the tuning in
    // OnConstruction, and gravity has to be right from the first simulated frame.
    LoadAndApplyCourseTuning();

    SpawnOctopus();
    SpawnCamera();
    BindGoalFlag();
    MaybeSpawnFallbackLight();

    RegisterArmParticipants();
}

void AOctoGameMode::LoadAndApplyCourseTuning()
{
    if (const UOctoTuningSubsystem* TuningSubsystem = UOctoTuningSubsystem::Get(this))
    {
        Tuning = TuningSubsystem->GetTuning();
    }

    // FBodyInstance has no per-body gravity scale in 5.7, so course gravity is a
    // world setting. bGlobalGravitySet is what makes GetGravityZ read
    // GlobalGravityZ instead of the project default.
    if (AWorldSettings* Settings = GetWorldSettings())
    {
        Settings->bGlobalGravitySet = true;
        Settings->GlobalGravityZ    = Tuning.WorldGravityZ;
    }

    // Inherited from APartyMinigameGameMode — set before the first Tick reads it.
    TutorialMaxSeconds = Tuning.TutorialMaxSeconds;
}

void AOctoGameMode::RegisterArmParticipants()
{
    // The tutorial's ready check waits on every registered participant, so
    // registering all eight arms unconditionally would make it unsatisfiable
    // for any party smaller than eight — only the timer could ever dismiss it.
    // Prefer the Lobby's actual roster; fall back to all eight arms only when
    // nobody registered (opening L_GameC directly), which mirrors
    // APartyArenaGameMode::SpawnPawns' dev-fallback rule.
    int32 NumRegistered = 0;
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        if (IsTileJoined(i))
        {
            RegisterParticipant(i, /*bIsAI=*/false); // no AI in this game — see class comment
            ++NumRegistered;
        }
    }

    if (NumRegistered > 0) { return; }

    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        RegisterParticipant(i, /*bIsAI=*/false);
    }
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
    SpawnLocation.X = Tuning.PlayPlaneX;

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

void AOctoGameMode::OnGameplayButton(int32 PlayerIndex)
{
    if (!Octo || PlayerIndex < 0 || PlayerIndex >= OctoArm::NumArms) { return; }
    if (bRequireRegistration && !IsTileJoined(PlayerIndex)) { return; }
    Octo->NotifyArmPressed(PlayerIndex);
}

void AOctoGameMode::OnGameplayButtonReleased(int32 PlayerIndex)
{
    if (!Octo || PlayerIndex < 0 || PlayerIndex >= OctoArm::NumArms) { return; }
    if (bRequireRegistration && !IsTileJoined(PlayerIndex)) { return; }
    Octo->NotifyArmReleased(PlayerIndex);
}

void AOctoGameMode::SetGameplayFrozen(bool bFrozen)
{
    if (Octo)
    {
        Octo->SetPhysicsFrozen(bFrozen);
    }
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

// ---- Dev tuning menu -------------------------------------------------------

void AOctoGameMode::OnDevMenuToggle()
{
    if (bDevMenuOpen)
    {
        // Tab out is the same as Cancel — close, keep the edits, write the ini.
        CloseDevMenu();
        UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: dev tuning menu closed."));
        return;
    }

    bDevMenuOpen = true;
    SetDevMenuInputMode(true);

    // Row 0 is always a category header, so a fresh menu would open with nothing
    // usable selected. Step forward to the first real row (a no-op when reopening
    // onto a row that's still selectable).
    const TArray<FPartyDevMenuRow> Rows = GetDevMenuRows();
    if (!Rows.IsValidIndex(DevMenuSelection) || Rows[DevMenuSelection].bIsHeader)
    {
        MoveDevMenuSelection(+1);
    }

    UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: dev tuning menu opened (%s)."),
        *UOctoTuningSubsystem::GetIniPath());
}

void AOctoGameMode::SetDevMenuInputMode(bool bMenuOpen)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) { return; }

    PC->bShowMouseCursor = bMenuOpen;

    // GameAndUI, not UIOnly: the arm buttons must keep working while the dialog
    // is up, because feeling a value change is the entire point of previewing it
    // live. bLockMouseToViewportBehavior stays at its default so the cursor can
    // leave the PIE window.
    if (bMenuOpen)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
    }
    else
    {
        PC->SetInputMode(FInputModeGameOnly());
    }
}

int32 AOctoGameMode::DevMenuRowToParamIndex(int32 RowIndex) const
{
    // Rows are params interleaved with a header per category change, so walk the
    // same sequence GetDevMenuRows builds and count.
    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();

    int32 Row = 0;
    const TCHAR* LastCategory = nullptr;

    for (int32 i = 0; i < Params.Num(); i++)
    {
        if (LastCategory == nullptr || FCString::Strcmp(LastCategory, Params[i].Category) != 0)
        {
            LastCategory = Params[i].Category;
            if (Row == RowIndex) { return INDEX_NONE; } // this row is the header
            Row++;
        }

        if (Row == RowIndex) { return i; }
        Row++;
    }

    return INDEX_NONE; // past the params: a blank or an action row
}

int32 AOctoGameMode::GetDevMenuAcceptRowIndex() const
{
    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();

    int32 NumHeaders = 0;
    const TCHAR* LastCategory = nullptr;
    for (const FOctoTuningParam& P : Params)
    {
        if (LastCategory == nullptr || FCString::Strcmp(LastCategory, P.Category) != 0)
        {
            LastCategory = P.Category;
            NumHeaders++;
        }
    }

    return Params.Num() + NumHeaders;
}

TArray<FPartyDevMenuRow> AOctoGameMode::GetDevMenuRows() const
{
    TArray<FPartyDevMenuRow> Rows;

    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();
    Rows.Reserve(Params.Num() + 8);

    const TCHAR* LastCategory = nullptr;
    for (const FOctoTuningParam& P : Params)
    {
        if (LastCategory == nullptr || FCString::Strcmp(LastCategory, P.Category) != 0)
        {
            LastCategory = P.Category;

            FPartyDevMenuRow Header;
            Header.Label     = FString(P.Category).ToUpper();
            Header.bIsHeader = true;
            Rows.Add(Header);
        }

        FPartyDevMenuRow Row;
        Row.Label      = P.Name;
        Row.ValueText  = OctoTuning::FormatValue(Tuning, P);
        Row.Normalized = OctoTuning::GetNormalized(Tuning, P);
        Row.Note       = P.bRequiresRestart ? TEXT("(restart)") : FString();
        Rows.Add(Row);
    }

    // Action rows, in the order ActivateDevMenuRow expects (Accept, Reset, Cancel).
    FPartyDevMenuRow Accept;
    Accept.Label     = TEXT("ACCEPT & RESTART");
    Accept.bIsAction = true;
    Rows.Add(Accept);

    FPartyDevMenuRow Reset;
    Reset.Label     = TEXT("RESET TO DEFAULTS");
    Reset.Note      = TEXT("(clears ini)");
    Reset.bIsAction = true;
    Rows.Add(Reset);

    FPartyDevMenuRow Cancel;
    Cancel.Label     = TEXT("CANCEL");
    Cancel.bIsAction = true;
    Rows.Add(Cancel);

    return Rows;
}

void AOctoGameMode::SetDevMenuSelection(int32 RowIndex)
{
    if (!bDevMenuOpen) { return; }

    const TArray<FPartyDevMenuRow> Rows = GetDevMenuRows();
    if (Rows.IsValidIndex(RowIndex) && !Rows[RowIndex].bIsHeader)
    {
        DevMenuSelection = RowIndex;
    }
}

void AOctoGameMode::MoveDevMenuSelection(int32 Delta)
{
    if (!bDevMenuOpen || Delta == 0) { return; }

    const TArray<FPartyDevMenuRow> Rows = GetDevMenuRows();
    if (Rows.IsEmpty()) { return; }

    // Step until we land on something selectable. Wrapping means the list has no
    // dead ends, and headers are never more than one row deep so this terminates
    // well before the guard.
    int32 Index = DevMenuSelection;
    for (int32 Guard = 0; Guard < Rows.Num(); Guard++)
    {
        Index = (Index + Delta + Rows.Num()) % Rows.Num();
        if (!Rows[Index].bIsHeader)
        {
            DevMenuSelection = Index;
            return;
        }
    }
}

void AOctoGameMode::NudgeDevMenuSelection(int32 Direction)
{
    if (!bDevMenuOpen) { return; }

    const int32 ParamIndex = DevMenuRowToParamIndex(DevMenuSelection);
    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();
    if (!Params.IsValidIndex(ParamIndex)) { return; } // an action row has no value

    OctoTuning::Nudge(Tuning, Params[ParamIndex], Direction);
    PushLiveTuning();
}

void AOctoGameMode::SetDevMenuRowNormalized(int32 RowIndex, float Alpha)
{
    if (!bDevMenuOpen) { return; }

    const int32 ParamIndex = DevMenuRowToParamIndex(RowIndex);
    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();
    if (!Params.IsValidIndex(ParamIndex)) { return; }

    DevMenuSelection = RowIndex; // dragging a row also selects it
    OctoTuning::SetNormalized(Tuning, Params[ParamIndex], Alpha);
    PushLiveTuning();
}

void AOctoGameMode::PushLiveTuning()
{
    // The subsystem is the copy that survives the reload, so it has to be written
    // on every change — not just on Accept. That is what makes Cancel keep your
    // edits (they simply haven't been rebuilt into the pawn yet).
    if (UOctoTuningSubsystem* TuningSubsystem = UOctoTuningSubsystem::Get(this))
    {
        TuningSubsystem->SetTuning(Tuning);
    }

    if (Octo)   { Octo->ApplyLiveTuning(Tuning); }
    if (Camera) { Camera->ApplyLiveTuning(Tuning); }

    if (AWorldSettings* Settings = GetWorldSettings())
    {
        Settings->bGlobalGravitySet = true;
        Settings->GlobalGravityZ    = Tuning.WorldGravityZ;
    }
}

void AOctoGameMode::ActivateDevMenuRow(int32 RowIndex)
{
    if (!bDevMenuOpen) { return; }

    const int32 AcceptRow = GetDevMenuAcceptRowIndex();

    if (RowIndex == AcceptRow)
    {
        // Paste this line straight back over FOctoTuning's member initialisers to
        // promote the tuning to a shipping default. The ini persists it in the
        // meantime, but the ini is an override — the header is the source of truth.
        UE_LOG(LogPartyButtons, Log, TEXT("OctoTuning: %s"), *OctoTuning::ToJsonString(Tuning));

        CloseDevMenu();
        ReloadCourse();
        return;
    }

    if (RowIndex == AcceptRow + 1) // Reset to defaults
    {
        // Stays open, so the values are visibly seen to snap back. ResetTuning
        // also empties the ini section — without that a stale override would keep
        // outranking a C++ default changed later, which is the whole hazard of
        // persisting to disk.
        if (UOctoTuningSubsystem* TuningSubsystem = UOctoTuningSubsystem::Get(this))
        {
            TuningSubsystem->ResetTuning();
            Tuning = TuningSubsystem->GetTuning();
        }
        else
        {
            Tuning = FOctoTuning();
        }

        // Deliberately not PushLiveTuning: that would write the defaults straight
        // back into the ini we just cleared.
        if (Octo)   { Octo->ApplyLiveTuning(Tuning); }
        if (Camera) { Camera->ApplyLiveTuning(Tuning); }
        return;
    }

    if (RowIndex == AcceptRow + 2) // Cancel
    {
        // Closes only — no course reload. The edits are still saved, so the next
        // Tab picks up exactly where this left off.
        CloseDevMenu();
        return;
    }

    // A value row was clicked rather than dragged — just select it.
    SetDevMenuSelection(RowIndex);
}

void AOctoGameMode::CloseDevMenu()
{
    bDevMenuOpen = false;
    SetDevMenuInputMode(false);

    // Persist on close, whichever way the menu was dismissed: a tuning pass is
    // worth keeping even when it wasn't applied, and losing an hour of slider
    // work to a mis-clicked Cancel would be its own bug.
    if (const UOctoTuningSubsystem* TuningSubsystem = UOctoTuningSubsystem::Get(this))
    {
        TuningSubsystem->SaveToIni();
    }
}

void AOctoGameMode::OnDevIncrement()
{
    if (bDevMenuOpen) { MoveDevMenuSelection(-1); return; }
    Super::OnDevIncrement();
}

void AOctoGameMode::OnDevDecrement()
{
    if (bDevMenuOpen) { MoveDevMenuSelection(+1); return; }
    Super::OnDevDecrement();
}

void AOctoGameMode::OnDevLeft()
{
    if (bDevMenuOpen) { NudgeDevMenuSelection(-1); return; }
    Super::OnDevLeft();
}

void AOctoGameMode::OnDevRight()
{
    if (bDevMenuOpen) { NudgeDevMenuSelection(+1); return; }
    Super::OnDevRight();
}
