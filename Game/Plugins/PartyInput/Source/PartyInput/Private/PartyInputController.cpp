#include "PartyInputController.h"
#include "PartyInput.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"

// FInputActionInstance is declared in EnhancedPlayerInput.h
#include "EnhancedPlayerInput.h"
#include "TimerManager.h"

// ---- Keyboard emulation key table -----------------------------------------

/*static*/ const TArray<FKey>& APartyInputController::GetKeyboardEmulationKeys()
{
    // Buttons 1–8:  a s d f j k l ;
    // Buttons 9–16: e r t g h u i o
    static const TArray<FKey> Keys = {
        EKeys::A,         EKeys::S,         EKeys::D,         EKeys::F,
        EKeys::J,         EKeys::K,         EKeys::L,         EKeys::Semicolon,
        EKeys::E,         EKeys::R,         EKeys::T,         EKeys::G,
        EKeys::H,         EKeys::U,         EKeys::I,         EKeys::O,
    };
    return Keys;
}

// ---- Gamepad face-button table --------------------------------------------

/*static*/ const TArray<FPartyGamepadMapping>& APartyInputController::GetGamepadFaceButtonMappings()
{
    // Compass order — see the header for why these indices and not others.
    static const TArray<FPartyGamepadMapping> Mappings = {
        { 0, EKeys::Gamepad_FaceButton_Top    }, // north (Y / triangle)
        { 2, EKeys::Gamepad_FaceButton_Right  }, // east  (B / circle)
        { 4, EKeys::Gamepad_FaceButton_Bottom }, // south (A / cross)
        { 6, EKeys::Gamepad_FaceButton_Left   }, // west  (X / square)
    };
    return Mappings;
}

// ---- Runtime input builder ------------------------------------------------

void APartyInputController::BuildButtonInputs()
{
    // Skip if already fully populated (e.g., by designer-assigned asset properties).
    // Main button + dev actions are included in the skip guard: if all are set, skip entirely.
    // DevMenuAction is deliberately absent from the guard — it doesn't exist in
    // Shipping, and gating the whole build on it would change behaviour by config.
    if (ButtonActions.Num() == NUM_BUTTONS && ButtonMappingContext != nullptr && MainButtonAction != nullptr
        && DevIncrementAction != nullptr && DevDecrementAction != nullptr
        && DevLeftAction != nullptr && DevRightAction != nullptr)
    {
        return;
    }

    UObject* Outer = this;

    // Build 16 boolean UInputActions, one per physical button.
    ButtonActions.Reset(NUM_BUTTONS);
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        IA->ValueType = EInputActionValueType::Boolean;
        ButtonActions.Add(IA);
    }

    // Build the 17th "main button" action — host/MC control, NOT in ButtonActions.
    // This preserves the 16-player invariant: GetButtonCount() and ResolveActionIndex
    // operate only on ButtonActions and are completely unaffected by this action.
    if (MainButtonAction == nullptr)
    {
        UInputAction* MainIA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        MainIA->ValueType = EInputActionValueType::Boolean;
        MainButtonAction = MainIA;
    }

    // Build the generic dev-only increment/decrement actions — always created (like the
    // main button) so the skip-guard above stays a simple non-null check; only the key
    // MAPPING below is gated by bEnableDevControls.
    if (DevIncrementAction == nullptr)
    {
        UInputAction* IncIA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        IncIA->ValueType = EInputActionValueType::Boolean;
        DevIncrementAction = IncIA;
    }
    if (DevDecrementAction == nullptr)
    {
        UInputAction* DecIA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        DecIA->ValueType = EInputActionValueType::Boolean;
        DevDecrementAction = DecIA;
    }
    if (DevLeftAction == nullptr)
    {
        UInputAction* LeftIA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        LeftIA->ValueType = EInputActionValueType::Boolean;
        DevLeftAction = LeftIA;
    }
    if (DevRightAction == nullptr)
    {
        UInputAction* RightIA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        RightIA->ValueType = EInputActionValueType::Boolean;
        DevRightAction = RightIA;
    }
#if !UE_BUILD_SHIPPING
    if (DevMenuAction == nullptr)
    {
        UInputAction* MenuIA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        MenuIA->ValueType = EInputActionValueType::Boolean;
        DevMenuAction = MenuIA;
    }
#endif

    // Build the mapping context.
    UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer, NAME_None, RF_Transient);

    // Primary mapping: button i -> GenericUSBController_Button(i+1).
    // These are the real USB HID joystick buttons exposed by the Teensy via RawInput.
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        const FName KeyName(*FString::Printf(TEXT("GenericUSBController_Button%d"), i + 1));
        IMC->MapKey(ButtonActions[i].Get(), FKey(KeyName));
    }

    // Main button: GenericUSBController_Button17.
    // NOTE: physical Button17 requires a Teensy firmware update. Until then, only
    // the dev keyboard mapping (Enter) works. See AI/deferred-manual-work.md.
    IMC->MapKey(MainButtonAction.Get(), FKey(TEXT("GenericUSBController_Button17")));

    // Dev emulation: also map keyboard keys so each button can be exercised
    // without the physical Teensy hardware. Both mappings fire the same action.
    if (bEnableKeyboardEmulation)
    {
        const TArray<FKey>& EmulKeys = GetKeyboardEmulationKeys();
        for (int32 i = 0; i < NUM_BUTTONS && i < EmulKeys.Num(); i++)
        {
            IMC->MapKey(ButtonActions[i].Get(), EmulKeys[i]);
        }

        // Main button dev key: Enter (go back = long-press, activate = tap).
        IMC->MapKey(MainButtonAction.Get(), EKeys::Enter);
    }

    // Gamepad face buttons: a third source for a subset of the same actions, so
    // a controller can drive the four "cardinal" buttons alongside the cabinet
    // and the keyboard. All three mappings fire the identical action, which is
    // what keeps HandleButtonPressed's contract device-agnostic.
    if (bEnableGamepadEmulation)
    {
        for (const FPartyGamepadMapping& Mapping : GetGamepadFaceButtonMappings())
        {
            if (ButtonActions.IsValidIndex(Mapping.ButtonIndex))
            {
                IMC->MapKey(ButtonActions[Mapping.ButtonIndex].Get(), Mapping.Key);
            }
        }
    }

    // Dev-only arrow + Tab keys — no physical arcade equivalent, gated by their own
    // flag (independent of bEnableKeyboardEmulation, since arrows aren't part of the
    // asdfjkl;ertghuio scheme). Tab is unbound elsewhere in the project, so there is
    // nothing to conflict with.
    if (bEnableDevControls)
    {
        IMC->MapKey(DevIncrementAction.Get(), EKeys::Up);
        IMC->MapKey(DevDecrementAction.Get(), EKeys::Down);
        IMC->MapKey(DevLeftAction.Get(),      EKeys::Left);
        IMC->MapKey(DevRightAction.Get(),     EKeys::Right);
#if !UE_BUILD_SHIPPING
        IMC->MapKey(DevMenuAction.Get(),      EKeys::Tab);
#endif
    }

    ButtonMappingContext = IMC;

    UE_LOG(LogPartyInput, Log,
        TEXT("PartyInputController: built %d player actions + 1 main button action + IMC at runtime%s%s%s."),
        NUM_BUTTONS,
        bEnableKeyboardEmulation ? TEXT(" (+ keyboard emulation: asdfkjl;ertghuio + Enter)") : TEXT(""),
        bEnableGamepadEmulation ? TEXT(" (+ gamepad face buttons -> 0/2/4/6)") : TEXT(""),
        bEnableDevControls ? TEXT(" (+ dev arrows/Tab)") : TEXT(""));
}

// ---- Lifecycle ------------------------------------------------------------

void APartyInputController::BeginPlay()
{
    Super::BeginPlay();

    // BuildButtonInputs() was already called from SetupInputComponent (which fires
    // before BeginPlay). Call here for the idempotent no-op skip — it also ensures
    // the IMC is built in edge-cases where SetupInputComponent hasn't fired yet.
    BuildButtonInputs();

    // Register the mapping context with Enhanced Input.
    // This must happen in BeginPlay (not SetupInputComponent) because the
    // UEnhancedInputLocalPlayerSubsystem requires a valid local player, which
    // is only guaranteed by the time BeginPlay fires.
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (ButtonMappingContext)
        {
            Subsystem->AddMappingContext(ButtonMappingContext, /*Priority*/ 0);
            UE_LOG(LogPartyInput, Log, TEXT("PartyInputController: mapping context added (priority 0)."));
        }
    }
    else
    {
        UE_LOG(LogPartyInput, Warning, TEXT("PartyInputController: no UEnhancedInputLocalPlayerSubsystem — is Enhanced Input enabled?"));
    }
}

void APartyInputController::SetupInputComponent()
{
    // Build actions first so ButtonActions is populated before we bind.
    // SetupInputComponent fires before BeginPlay, so this is where we do it.
    BuildButtonInputs();

    Super::SetupInputComponent();

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC)
    {
        UE_LOG(LogPartyInput, Warning, TEXT("PartyInputController: InputComponent is not UEnhancedInputComponent. Is Enhanced Input enabled?"));
        return;
    }

    for (const TObjectPtr<UInputAction>& Action : ButtonActions)
    {
        if (Action)
        {
            EIC->BindAction(Action, ETriggerEvent::Started,   this,
                            &APartyInputController::OnAnyButton);
            EIC->BindAction(Action, ETriggerEvent::Completed, this,
                            &APartyInputController::OnAnyButtonReleased);
        }
    }

    // Bind the main button to its own Started/Completed pair for tap-vs-hold classification.
    // Never routes through OnAnyButton — the main button has no PlayerIndex.
    if (MainButtonAction)
    {
        EIC->BindAction(MainButtonAction, ETriggerEvent::Started,   this,
                        &APartyInputController::OnMainButtonStarted);
        EIC->BindAction(MainButtonAction, ETriggerEvent::Completed, this,
                        &APartyInputController::OnMainButtonCompleted);
    }

    // Dev-only increment/decrement — a press is a single nudge, no hold semantics.
    if (DevIncrementAction)
    {
        EIC->BindAction(DevIncrementAction, ETriggerEvent::Started, this,
                        &APartyInputController::OnDevIncrementStarted);
    }
    if (DevDecrementAction)
    {
        EIC->BindAction(DevDecrementAction, ETriggerEvent::Started, this,
                        &APartyInputController::OnDevDecrementStarted);
    }
    if (DevLeftAction)
    {
        EIC->BindAction(DevLeftAction, ETriggerEvent::Started, this,
                        &APartyInputController::OnDevLeftStarted);
    }
    if (DevRightAction)
    {
        EIC->BindAction(DevRightAction, ETriggerEvent::Started, this,
                        &APartyInputController::OnDevRightStarted);
    }
#if !UE_BUILD_SHIPPING
    if (DevMenuAction)
    {
        EIC->BindAction(DevMenuAction, ETriggerEvent::Started, this,
                        &APartyInputController::OnDevMenuStarted);
    }
#endif

    UE_LOG(LogPartyInput, Log, TEXT("PartyInputController: bound %d player actions + main button."), ButtonActions.Num());
}

// ---- Dispatch -------------------------------------------------------------

int32 APartyInputController::ResolveActionIndex(const UInputAction* Action) const
{
    return ButtonActions.IndexOfByKey(Action);
}

void APartyInputController::OnAnyButton(const FInputActionInstance& Instance)
{
    const UInputAction* Src = Instance.GetSourceAction();
    const int32 Index = ResolveActionIndex(Src);
    if (Index != INDEX_NONE)
    {
        HandleButtonPressed(Index);
    }
}

void APartyInputController::OnAnyButtonReleased(const FInputActionInstance& Instance)
{
    const UInputAction* Src = Instance.GetSourceAction();
    const int32 Index = ResolveActionIndex(Src);
    if (Index != INDEX_NONE)
    {
        HandleButtonReleased(Index);
    }
}

void APartyInputController::HandleButtonPressed(int32 PlayerIndex)
{
    UE_LOG(LogPartyInput, Log, TEXT("Party button pressed: player %d"), PlayerIndex + 1);
    OnButtonPressed.Broadcast(PlayerIndex);
}

void APartyInputController::HandleButtonReleased(int32 PlayerIndex)
{
    UE_LOG(LogPartyInput, Log, TEXT("Party button released: player %d"), PlayerIndex + 1);
    OnButtonReleased.Broadcast(PlayerIndex);
}

// ---- Main button (tap vs long-press) -------------------------------------

void APartyInputController::OnMainButtonStarted(const FInputActionInstance& /*Instance*/)
{
    // Record the time and start the hold timer. If it fires before release, that's a hold.
    bMainHoldFired = false;
    MainButtonPressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    GetWorldTimerManager().SetTimer(
        MainHoldTimerHandle,
        this,
        &APartyInputController::OnMainHoldTimerFired,
        MainButtonHoldThreshold,
        /*bLooping=*/false);
}

void APartyInputController::OnMainButtonCompleted(const FInputActionInstance& /*Instance*/)
{
    // Cancel the hold timer — if it hasn't fired, this is a tap.
    GetWorldTimerManager().ClearTimer(MainHoldTimerHandle);

    if (!bMainHoldFired)
    {
        HandleMainButtonTapped();
    }
    // If bMainHoldFired is true, HandleMainButtonHeld() was already called by the timer.
}

void APartyInputController::OnMainHoldTimerFired()
{
    bMainHoldFired = true;
    HandleMainButtonHeld();
}

void APartyInputController::HandleMainButtonTapped()
{
    UE_LOG(LogPartyInput, Log, TEXT("Main button tapped (activate/confirm)."));
    OnMainButtonTapped.Broadcast();
}

void APartyInputController::HandleMainButtonHeld()
{
    UE_LOG(LogPartyInput, Log, TEXT("Main button held (back/up)."));
    OnMainButtonHeld.Broadcast();
}

// ---- Dev-only increment/decrement ------------------------------------------

void APartyInputController::OnDevIncrementStarted(const FInputActionInstance& /*Instance*/)
{
    HandleDevIncrement();
}

void APartyInputController::OnDevDecrementStarted(const FInputActionInstance& /*Instance*/)
{
    HandleDevDecrement();
}

void APartyInputController::HandleDevIncrement()
{
    UE_LOG(LogPartyInput, Log, TEXT("Dev increment pressed."));
    OnDevIncrement.Broadcast();
}

void APartyInputController::HandleDevDecrement()
{
    UE_LOG(LogPartyInput, Log, TEXT("Dev decrement pressed."));
    OnDevDecrement.Broadcast();
}

void APartyInputController::OnDevLeftStarted(const FInputActionInstance& /*Instance*/)
{
    HandleDevLeft();
}

void APartyInputController::OnDevRightStarted(const FInputActionInstance& /*Instance*/)
{
    HandleDevRight();
}

void APartyInputController::HandleDevLeft()
{
    UE_LOG(LogPartyInput, Log, TEXT("Dev left pressed."));
    OnDevLeft.Broadcast();
}

void APartyInputController::HandleDevRight()
{
    UE_LOG(LogPartyInput, Log, TEXT("Dev right pressed."));
    OnDevRight.Broadcast();
}

#if !UE_BUILD_SHIPPING
void APartyInputController::OnDevMenuStarted(const FInputActionInstance& /*Instance*/)
{
    HandleDevMenu();
}

void APartyInputController::HandleDevMenu()
{
    UE_LOG(LogPartyInput, Log, TEXT("Dev menu key pressed."));
    OnDevMenu.Broadcast();
}
#endif
