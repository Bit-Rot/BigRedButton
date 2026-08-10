#include "OctoOdyssey/OctoGameMode.h"
#include "PartyButtons.h"
#include "OctoOdyssey/OctoPawn.h"
#include "OctoOdyssey/OctoCamera.h"
#include "OctoOdyssey/OctoGoalFlag.h"
#include "OctoOdyssey/OctoSpawnPoint.h"
#include "OctoOdyssey/OctoViewPoint.h"
#include "OctoOdyssey/OctoPlayerController.h"
#include "OctoOdyssey/OctoHUD.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "OctoOdyssey/OctoTuningSubsystem.h"
#include "OctoOdyssey/OctoScoreSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "TimerManager.h"

namespace
{
    /** Menu rows, in display order. Index maps to the switch in ActivateMenuOption. */
    const TCHAR* const GMenuOptions[AOctoGameMode::NumMenuOptions] = {
        TEXT("START GAME"),
        TEXT("START GAME (HARD)"),
        TEXT("TOP SCORES"),
    };
}

AOctoGameMode::AOctoGameMode()
{
    // Ticks for the name-entry hold-repeat. Nothing else here needs a tick — the
    // intro state machine that used to justify one left with APartyMinigameGameMode.
    PrimaryActorTick.bCanEverTick = true;

    DefaultPawnClass      = nullptr; // the octopus is spawned manually and never possessed
    PlayerControllerClass = AOctoPlayerController::StaticClass(); // 1s main-button hold — see its class comment
    HUDClass              = AOctoHUD::StaticClass();

    OctoPawnClass   = AOctoPawn::StaticClass();
    OctoCameraClass = AOctoCamera::StaticClass();
}

/*static*/ const TCHAR* AOctoGameMode::GetMenuOptionLabel(int32 OptionIndex)
{
    return (OptionIndex >= 0 && OptionIndex < NumMenuOptions) ? GMenuOptions[OptionIndex] : TEXT("");
}

FString AOctoGameMode::GetHudSubtitle() const
{
    return TEXT("8 buttons = 8 arms. Push off the world. Reach the flag.");
}

// ---- Setup -----------------------------------------------------------------

void AOctoGameMode::BeginPlay()
{
    Super::BeginPlay();

    LoadAndApplyCourseTuning();

    MenuViewPoint = FindMenuViewPoint();
    BindGoalFlags();
    MaybeSpawnFallbackLight();

    PendingName = OctoScores::BlankName();

    // Immediate, not blended: there is nothing to blend FROM on the first frame,
    // and a blend from the default auto-view target reads as a stray camera swoop
    // every time the level opens.
    FlowState = EOctoFlowState::MainMenu;
    SetViewTo(MenuViewPoint, /*bImmediate=*/true);
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
}

void AOctoGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // Re-assert the view target here too — removes the BeginPlay-vs-PostLogin
    // ordering dependency, for the cost of one idempotent call. Whichever actor
    // the current state points at is the right one.
    if (!NewPlayer) { return; }

    AActor* Target = (FlowState == EOctoFlowState::Playing) ? Cast<AActor>(Camera) : Cast<AActor>(MenuViewPoint);
    if (Target)
    {
        NewPlayer->SetViewTarget(Target);
    }
}

AOctoViewPoint* AOctoGameMode::FindMenuViewPoint() const
{
    if (!GetWorld()) { return nullptr; }

    for (TActorIterator<AOctoViewPoint> It(GetWorld()); It; ++It)
    {
        if (It->GetViewRole() == EOctoViewRole::MainMenu)
        {
            return *It;
        }
    }

    UE_LOG(LogPartyButtons, Warning,
        TEXT("AOctoGameMode: no AOctoViewPoint with role MainMenu placed — the menu has no camera."));
    return nullptr;
}

void AOctoGameMode::BindGoalFlags()
{
    if (!GetWorld()) { return; }

    // Every flag, not just the first: there is one per course, and binding only
    // one would leave the other course uncompletable. (The old single-flag,
    // single-map version returned after the first hit.)
    int32 NumBound = 0;
    for (TActorIterator<AOctoGoalFlag> It(GetWorld()); It; ++It)
    {
        It->OnReached.AddUObject(this, &AOctoGameMode::HandleGoalReached);
        ++NumBound;
    }

    if (NumBound == 0)
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("AOctoGameMode: no AOctoGoalFlag found in the level — no course can be won."));
    }
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

// ---- Flow ------------------------------------------------------------------

FVector AOctoGameMode::FindSpawnLocation(EOctoCourse Course) const
{
    if (GetWorld())
    {
        for (TActorIterator<AOctoSpawnPoint> It(GetWorld()); It; ++It)
        {
            if (It->GetCourse() == Course)
            {
                // X included, deliberately. It used to be overwritten with
                // Tuning.PlayPlaneX so an editor-nudged spawn point couldn't pin
                // the octopus off the plane the camera framed. With two courses on
                // two different planes in one level that guard is inverted: the
                // spawn point IS the plane, and the camera is told to frame the
                // same number (see EnterCourse), so the two cannot desync.
                return It->GetActorLocation();
            }
        }
    }

    UE_LOG(LogPartyButtons, Warning,
        TEXT("AOctoGameMode: no AOctoSpawnPoint tagged %s — falling back to the play plane."),
        OctoCourse::ToString(Course));

    FVector Fallback = FallbackSpawnLocation;
    Fallback.X = Tuning.PlayPlaneX;
    return Fallback;
}

void AOctoGameMode::EnterCourse(EOctoCourse Course)
{
    if (!GetWorld() || !OctoPawnClass || !OctoCameraClass) { return; }

    ActiveCourse = Course;

    const FVector SpawnLocation = FindSpawnLocation(Course);

    // Re-arm both flags. The flag's one-shot used to be cleared by the level
    // reload that reaching it triggered; nothing reloads any more, so without this
    // the second run of a session could never be finished.
    for (TActorIterator<AOctoGoalFlag> It(GetWorld()); It; ++It)
    {
        It->ResetTrigger();
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    Octo = GetWorld()->SpawnActor<AOctoPawn>(OctoPawnClass, FTransform(SpawnLocation), Params);
    if (!Octo)
    {
        UE_LOG(LogPartyButtons, Warning, TEXT("AOctoGameMode: failed to spawn AOctoPawn."));
        return;
    }

    Camera = GetWorld()->SpawnActor<AOctoCamera>(OctoCameraClass, FTransform::Identity);
    if (!Camera)
    {
        // Back out cleanly rather than leaving an unwatchable octopus loose in the
        // level: without a camera the players would have no view of it and no way
        // back to the menu bar the abort hold.
        UE_LOG(LogPartyButtons, Warning, TEXT("AOctoGameMode: failed to spawn AOctoCamera."));
        LeaveCourse();
        return;
    }

    Camera->SetFollowTarget(Octo);
    Camera->SetPlayPlaneX(SpawnLocation.X); // one number, shared with the pawn — see FindSpawnLocation
    Camera->SnapToTarget();                 // before the blend, or the blend chases a moving target

    SetViewTo(Camera, /*bImmediate=*/false);

    // No countdown and no tutorial: the octopus is live the instant it exists, and
    // the clock starts with it. The view blend runs over the top of live play.
    FlowState    = EOctoFlowState::Playing;
    RunStartTime = GetWorld()->GetTimeSeconds();

    UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: starting the %s course at %s."),
        OctoCourse::ToString(Course), *SpawnLocation.ToCompactString());
}

void AOctoGameMode::LeaveCourse()
{
    // The octopus goes immediately. The camera stops following the moment its
    // target is gone (AOctoCamera::Tick early-outs on an invalid target), so it
    // simply freezes where it was — which is exactly the shot you want the blend
    // back to the menu to start from.
    if (Octo)
    {
        Octo->Destroy();
        Octo = nullptr;
    }

    LetterHolds.Reset();

    if (!Camera) { return; }

    // The camera OUTLIVES this call by the length of the blend.
    //
    // Destroying it here would destroy the player's current view target, and
    // SetViewTargetWithBlend cannot blend FROM a dead actor — it snaps. That
    // turns the one transition the whole menu flow is built around into a hard
    // cut. Releasing our reference now (so EnterCourse can spawn a fresh one
    // straight away) and destroying the old actor once the blend has landed
    // keeps the move smooth in both directions.
    TWeakObjectPtr<AOctoCamera> Departing = Camera;
    Camera = nullptr;

    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle, [Departing]()
    {
        if (Departing.IsValid())
        {
            Departing->Destroy();
        }
    }, FMath::Max(ViewBlendSeconds, 0.01f) + 0.1f, /*bLoop=*/false);
}

void AOctoGameMode::ReturnToMenu()
{
    LeaveCourse();

    // Cleared here rather than on entry to ScoreEntry: this is the one path every
    // exit from a score screen goes through, so the temporary slot-11 row can
    // never survive into the next visit.
    PendingRank     = INDEX_NONE;
    PendingName     = OctoScores::BlankName();
    FinishedSeconds = 0.f;

    FlowState = EOctoFlowState::MainMenu;
    SetViewTo(MenuViewPoint, /*bImmediate=*/false);
}

void AOctoGameMode::HandleGoalReached(EOctoCourse Course)
{
    // Both courses live in one level, so a flag can fire while the OTHER course is
    // the one being played (a stray physics overlap, a respawn landing in a
    // trigger). Only the flag belonging to the run in progress ends it.
    if (FlowState != EOctoFlowState::Playing || Course != ActiveCourse)
    {
        return;
    }

    FinishedSeconds = GetRunSeconds();

    PendingName = OctoScores::BlankName();
    PendingRank = INDEX_NONE;
    if (const UOctoScoreSubsystem* Scores = UOctoScoreSubsystem::Get(this))
    {
        PendingRank = Scores->FindInsertRank(ActiveCourse, FinishedSeconds);
    }

    // State flips NOW, so the buttons stop driving arms on this very frame and
    // start driving name entry.
    FlowState = EOctoFlowState::ScoreEntry;

    // Teardown is deferred one tick. This runs from inside AOctoGoalFlag's
    // overlap callback, i.e. during this tick's physics/overlap processing, and
    // destroying the simulating pawn from in there is the same hazard the old
    // ReloadCourse deferred its OpenLevel for.
    GetWorldTimerManager().SetTimerForNextTick([this]()
    {
        LeaveCourse();
        SetViewTo(MenuViewPoint, /*bImmediate=*/false);
    });

    UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: %s course finished in %s — %s."),
        OctoCourse::ToString(ActiveCourse), *OctoScores::FormatTime(FinishedSeconds),
        PendingRank == INDEX_NONE
            ? TEXT("missed the top ten")
            : *FString::Printf(TEXT("rank %d"), PendingRank + 1));
}

void AOctoGameMode::CommitPendingScore()
{
    // A run that missed the top ten has nothing to store — its slot-11 row is a
    // HUD fiction (see OctoScores::NumSlots) and is simply dropped.
    if (PendingRank != INDEX_NONE)
    {
        if (UOctoScoreSubsystem* Scores = UOctoScoreSubsystem::Get(this))
        {
            Scores->SubmitScore(ActiveCourse, FOctoScoreEntry(PendingName, FinishedSeconds));
        }
    }

    ReturnToMenu();
}

void AOctoGameMode::RestartRun()
{
    if (FlowState != EOctoFlowState::Playing) { return; }

    // Respawn rather than reload. The old ReloadCourse() called OpenLevel because
    // geometry tuning is baked in at spawn (AOctoPawn::OnConstruction) and there
    // was no other way to rebuild the pawn. A fresh SpawnActor rebuilds it just as
    // completely, and keeps the level — and the scoreboard — intact.
    const EOctoCourse Course = ActiveCourse;
    LeaveCourse();
    EnterCourse(Course);
}

void AOctoGameMode::SetViewTo(AActor* Target, bool bImmediate)
{
    if (!Target || !GetWorld()) { return; }

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) { return; }

    if (bImmediate)
    {
        PC->SetViewTarget(Target);
        return;
    }

    // Cubic rather than linear: the ease at both ends is what makes the move
    // between islands read as a camera and not as a teleport with extra steps.
    PC->SetViewTargetWithBlend(Target, ViewBlendSeconds, EViewTargetBlendFunction::VTBlend_Cubic);
}

float AOctoGameMode::GetRunSeconds() const
{
    switch (FlowState)
    {
    case EOctoFlowState::Playing:
        return GetWorld() ? FMath::Max(0.f, GetWorld()->GetTimeSeconds() - RunStartTime) : 0.f;

    case EOctoFlowState::ScoreEntry:
        return FinishedSeconds;

    default:
        return 0.f;
    }
}

const TArray<FOctoScoreEntry>& AOctoGameMode::GetScoreTable(EOctoCourse Course) const
{
    if (const UOctoScoreSubsystem* Scores = UOctoScoreSubsystem::Get(this))
    {
        return Scores->GetTable(Course);
    }

    static const TArray<FOctoScoreEntry> Empty;
    return Empty;
}

void AOctoGameMode::ActivateMenuOption()
{
    switch (MenuSelection)
    {
    case 0:
        EnterCourse(EOctoCourse::Normal);
        break;

    case 1:
        EnterCourse(EOctoCourse::Hard);
        break;

    case 2:
        // Read-only: no PendingRank is set, so the HUD draws both tables with no
        // editable row and the header reads TOP SCORES.
        FlowState = EOctoFlowState::ScoreView;
        break;

    default:
        break;
    }
}

// ---- Input -----------------------------------------------------------------

void AOctoGameMode::OnPlayerButton(int32 PlayerIndex)
{
    switch (FlowState)
    {
    case EOctoFlowState::MainMenu:
        // ANY button cycles — with sixteen of them on a cabinet and no cursor
        // keys, "everyone can drive the menu" is the only workable rule.
        MenuSelection = (MenuSelection + 1) % NumMenuOptions;
        break;

    case EOctoFlowState::Playing:
        if (Octo && PlayerIndex >= 0 && PlayerIndex < OctoArm::NumArms)
        {
            Octo->NotifyArmPressed(PlayerIndex);
        }
        break;

    case EOctoFlowState::ScoreEntry:
        // Player i owns letter i, unconditionally — no claiming, no cursor. The
        // tap advances one glyph; holding starts the auto-repeat in TickNameEntry.
        if (PlayerIndex >= 0 && PlayerIndex < OctoScores::NameLength)
        {
            CycleNameLetter(PlayerIndex);
            LetterHolds.Add(PlayerIndex, FLetterHold{ 0.f, NameHoldDelaySeconds });
        }
        break;

    case EOctoFlowState::ScoreView:
        break; // read-only; only the main button does anything here
    }
}

void AOctoGameMode::OnPlayerButtonReleased(int32 PlayerIndex)
{
    switch (FlowState)
    {
    case EOctoFlowState::Playing:
        if (Octo && PlayerIndex >= 0 && PlayerIndex < OctoArm::NumArms)
        {
            Octo->NotifyArmReleased(PlayerIndex);
        }
        break;

    case EOctoFlowState::ScoreEntry:
        LetterHolds.Remove(PlayerIndex);
        break;

    default:
        break;
    }
}

void AOctoGameMode::OnMainTap()
{
    switch (FlowState)
    {
    case EOctoFlowState::MainMenu:
        ActivateMenuOption();
        break;

    case EOctoFlowState::ScoreView:
        ReturnToMenu();
        break;

    case EOctoFlowState::ScoreEntry:
        // Deliberately nothing. Committing a name is a HOLD (see
        // AOctoPlayerController): a tap here would be one stray press away from
        // locking in a half-typed name, and there is no way back from that.
        break;

    default:
        break;
    }
}

void AOctoGameMode::OnMainHold()
{
    switch (FlowState)
    {
    case EOctoFlowState::ScoreEntry:
        CommitPendingScore();
        break;

    case EOctoFlowState::Playing:
        // Abort. A QWOP octopus can wedge itself somewhere unrecoverable, and
        // without this the only way out of a stuck run is to quit the game.
        UE_LOG(LogPartyButtons, Log, TEXT("AOctoGameMode: run abandoned — back to the menu."));
        ReturnToMenu();
        break;

    case EOctoFlowState::ScoreView:
        ReturnToMenu();
        break;

    default:
        break;
    }
}

// ---- Name entry ------------------------------------------------------------

void AOctoGameMode::CycleNameLetter(int32 LetterIndex)
{
    // Nothing to type when the run missed the board — the slot-11 row is a
    // read-only consolation, not an entry.
    if (PendingRank == INDEX_NONE) { return; }

    if (PendingName.Len() != OctoScores::NameLength)
    {
        PendingName = OctoScores::SanitizeName(PendingName); // restores the invariant
    }

    if (!PendingName.IsValidIndex(LetterIndex)) { return; }

    PendingName[LetterIndex] = OctoScores::CycleLetter(PendingName[LetterIndex], +1);
}

void AOctoGameMode::TickNameEntry(float DeltaSeconds)
{
    if (LetterHolds.IsEmpty()) { return; }

    const float RepeatInterval = (NameHoldRepeatHz > 0.f) ? (1.f / NameHoldRepeatHz) : 1.f;

    for (TPair<int32, FLetterHold>& Pair : LetterHolds)
    {
        FLetterHold& Hold = Pair.Value;
        Hold.Elapsed += DeltaSeconds;

        // A while, not an if: a hitched frame longer than the interval should
        // still advance the right NUMBER of letters rather than swallow them.
        while (Hold.Elapsed >= Hold.NextRepeatAt)
        {
            CycleNameLetter(Pair.Key);
            Hold.NextRepeatAt += RepeatInterval;
        }
    }
}

void AOctoGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (FlowState == EOctoFlowState::ScoreEntry)
    {
        TickNameEntry(DeltaSeconds);
    }
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
    Accept.Label     = TEXT("ACCEPT & RESPAWN");
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
    // The subsystem is the copy that survives a respawn, so it has to be written
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
        RestartRun(); // respawn, not reload — see RestartRun
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
        // Closes only — no respawn. The edits are still saved, so the next Tab
        // picks up exactly where this left off.
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
