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

// ---- Keyboard emulation key table -----------------------------------------

/*static*/ const TArray<FKey>& APartyInputController::GetKeyboardEmulationKeys()
{
    // Buttons 1–16 mapped to: a w s e d r f g h j i k o l p ;
    static const TArray<FKey> Keys = {
        EKeys::A, EKeys::W, EKeys::S, EKeys::E,
        EKeys::D, EKeys::R, EKeys::F, EKeys::G,
        EKeys::H, EKeys::U, EKeys::J, EKeys::I,
        EKeys::K, EKeys::O, EKeys::L, EKeys::P,
    };
    return Keys;
}

// ---- Runtime input builder ------------------------------------------------

void APartyInputController::BuildButtonInputs()
{
    // Skip if already populated (e.g., by designer-assigned asset properties).
    if (ButtonActions.Num() == NUM_BUTTONS && ButtonMappingContext != nullptr)
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

    // Build the mapping context.
    UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer, NAME_None, RF_Transient);

    // Primary mapping: button i -> GenericUSBController_Button(i+1).
    // These are the real USB HID joystick buttons exposed by the Teensy via RawInput.
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        const FName KeyName(*FString::Printf(TEXT("GenericUSBController_Button%d"), i + 1));
        IMC->MapKey(ButtonActions[i].Get(), FKey(KeyName));
    }

    // Dev emulation: also map keyboard keys so each button can be exercised
    // without the physical Teensy hardware. Both mappings fire the same action.
    if (bEnableKeyboardEmulation)
    {
        const TArray<FKey>& EmulKeys = GetKeyboardEmulationKeys();
        for (int32 i = 0; i < NUM_BUTTONS && i < EmulKeys.Num(); i++)
        {
            IMC->MapKey(ButtonActions[i].Get(), EmulKeys[i]);
        }
    }

    ButtonMappingContext = IMC;

    UE_LOG(LogPartyInput, Log,
        TEXT("PartyInputController: built %d actions + IMC at runtime%s."),
        NUM_BUTTONS,
        bEnableKeyboardEmulation ? TEXT(" (+ keyboard emulation: awsedrfghujikolp)") : TEXT(""));
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
            EIC->BindAction(Action, ETriggerEvent::Started, this,
                            &APartyInputController::OnAnyButton);
        }
    }

    UE_LOG(LogPartyInput, Log, TEXT("PartyInputController: bound %d actions."), ButtonActions.Num());
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

void APartyInputController::HandleButtonPressed(int32 PlayerIndex)
{
    // PlayerIndex 0 => player 1 / button 1, etc.
    UE_LOG(LogPartyInput, Log, TEXT("Party button pressed: player %d"), PlayerIndex + 1);

    // Broadcast so HUD, GameMode, and other observers can react.
    OnButtonPressed.Broadcast(PlayerIndex);

    // TODO (game owner): route this press to player PlayerIndex.
}
