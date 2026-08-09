#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Engine/TimerHandle.h"
#include "PartyInputController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionInstance;

/**
 * Delegate broadcast by HandleButtonPressed.
 * PlayerIndex is 0-based (0 = first button / player 1).
 * Bind to this in the HUD, GameMode, or any observer to react to button presses.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyButtonPressed, int32 /*PlayerIndex*/);

/**
 * Delegates broadcast by HandleMainButtonTapped / HandleMainButtonHeld.
 * The "main button" is button 17 — the host/MC control outside the 16-player set.
 * Dev key: Enter. Physical: GenericUSBController_Button17 (requires Teensy firmware bump).
 * No params — this is a singleton button with no player-index semantics.
 */
DECLARE_MULTICAST_DELEGATE(FOnPartyMainButton);

/**
 * Delegate broadcast by the generic dev-only increment/decrement controls
 * (Up/Down arrow keys). No params — this plugin only knows "a dev nudge
 * happened"; what it MEANS (e.g. adjusting an AI player count in the Lobby)
 * is entirely up to whichever game-layer code binds to it. See bEnableDevControls.
 */
DECLARE_MULTICAST_DELEGATE(FOnPartyDevKey);

/**
 * One gamepad key bound to one player-button index. Sparse by nature — a
 * gamepad has nowhere near 16 buttons, so only some indices are covered.
 * See APartyInputController::GetGamepadFaceButtonMappings.
 */
struct FPartyGamepadMapping
{
    int32 ButtonIndex = INDEX_NONE;
    FKey  Key;
};

/**
 * APartyInputController
 *
 * Fans 16 physical USB HID buttons through Unreal Enhanced Input to a single
 * indexed dispatch function: HandleButtonPressed(int32 PlayerIndex).
 *
 * Design (see AI/design/architecture.md):
 *   The 16 UInputAction objects and the UInputMappingContext are built at runtime
 *   in C++ (BuildButtonInputs), so the plugin works with zero hand-authored
 *   .uasset files. Designers who prefer Blueprint assets can assign
 *   ButtonMappingContext and ButtonActions via the EditDefaultsOnly properties;
 *   the runtime build is skipped when they are already set.
 *
 * Handoff point:
 *   HandleButtonPressed(int32 PlayerIndex) — override or bind OnButtonPressed.
 *   Everything past this point is the game owner's responsibility.
 *
 * Player model:
 *   One PC, one controller, 16 buttons. PlayerIndex is a dispatch INDEX, not
 *   a ULocalPlayer. Do not create 16 local players.
 */
UCLASS()
class PARTYINPUT_API APartyInputController : public APlayerController
{
    GENERATED_BODY()

public:
    /** Broadcast every time one of the 16 player buttons is pressed. PlayerIndex is 0-based. */
    FOnPartyButtonPressed OnButtonPressed;

    /** Broadcast every time one of the 16 player buttons is released. PlayerIndex is 0-based. */
    FOnPartyButtonPressed OnButtonReleased;

    /**
     * Broadcast when the main button (button 17 / Enter) is tapped (short press).
     * Use this to activate / confirm the currently selected menu option.
     */
    FOnPartyMainButton OnMainButtonTapped;

    /**
     * Broadcast when the main button is held long enough (>= MainButtonHoldThreshold).
     * Use this to go back / up one level.
     */
    FOnPartyMainButton OnMainButtonHeld;

    /** Broadcast when the dev-only "increment" key (Up arrow) is pressed. See bEnableDevControls. */
    FOnPartyDevKey OnDevIncrement;

    /** Broadcast when the dev-only "decrement" key (Down arrow) is pressed. See bEnableDevControls. */
    FOnPartyDevKey OnDevDecrement;

    /** Broadcast when the dev-only "left" key (Left arrow) is pressed. See bEnableDevControls. */
    FOnPartyDevKey OnDevLeft;

    /** Broadcast when the dev-only "right" key (Right arrow) is pressed. See bEnableDevControls. */
    FOnPartyDevKey OnDevRight;

    /**
     * Broadcast when the dev menu key (Tab) is pressed. Like the nudge keys, this
     * plugin only knows "the dev menu key happened" — what it opens is up to the
     * game layer (see APartyGameModeBase::OnDevMenuToggle).
     *
     * The Tab key mapping and the input binding are both compiled out of a
     * Shipping build, so this delegate can never fire there: a developer tuning
     * menu must not be reachable on a cabinet in the wild, and a runtime flag
     * someone can flip is a weaker guarantee than code that isn't there.
     */
    FOnPartyDevKey OnDevMenu;

    /**
     * Pure, world-free classification rule: held if HeldSeconds >= Threshold.
     * Unit-testable without a controller instance.
     */
    static bool IsHoldGesture(float HeldSeconds, float HoldThresholdSeconds)
    {
        return HeldSeconds >= HoldThresholdSeconds;
    }

    /**
     * Build the 16 UInputActions and the UInputMappingContext at runtime.
     * Called automatically in BeginPlay when ButtonActions is empty.
     * May also be called standalone (e.g., from automation tests) when an
     * Outer object is provided. Pass GetTransientPackage() in tests.
     *
     * After a successful call, ButtonActions contains 16 non-null distinct
     * UInputAction objects and ButtonMappingContext is populated.
     */
    UFUNCTION(BlueprintCallable, Category = "PartyInput")
    void BuildButtonInputs();

    /**
     * Resolve the array index for a given source action.
     * Returns INDEX_NONE if the action is not in ButtonActions.
     * This is the exact lookup used in OnAnyButton — test it here.
     */
    int32 ResolveActionIndex(const UInputAction* Action) const;

    /** Returns the number of actions currently in ButtonActions. */
    int32 GetButtonCount() const { return ButtonActions.Num(); }

    /** Returns the built mapping context (may be null before BeginPlay/BuildButtonInputs). */
    UInputMappingContext* GetButtonMappingContext() const { return ButtonMappingContext; }

    /**
     * Returns the 16 keyboard keys mapped to buttons 1–16 for dev emulation.
     * Order: 1-8 = asdfjkl;   9-16 = ertghuio
     * Used by BuildButtonInputs and the demo HUD. Static so it can be called
     * without a controller instance (e.g., in tests or the HUD).
     */
    static const TArray<FKey>& GetKeyboardEmulationKeys();

    /**
     * The four gamepad face buttons, mapped to the player buttons that sit in the
     * matching compass direction on screen.
     *
     * The layout is OctoOdyssey's: its eight arms are evenly spaced around the
     * body in the Y-Z play plane, at angles offset + 45*i measured from +Z, with
     * arm direction (0, sin, cos) — and the camera is unrotated, so +Z is screen
     * up and +Y is screen right. That puts arms 0/2/4/6 at north/east/south/west,
     * which is exactly the face buttons' arrangement:
     *
     *     Top    (Y / triangle)  -> button 0  (north)
     *     Right  (B / circle)    -> button 2  (east)
     *     Bottom (A / cross)     -> button 4  (south)
     *     Left   (X / square)    -> button 6  (west)
     *
     * The ordering survives retuning ArmAngleOffsetDegrees: rotating the whole
     * arm ring turns all four together, so 0/2/4/6 stay 90 degrees apart in the
     * same rotational order and the compass assignment stays consistent.
     *
     * Static so it can be called without a controller instance (tests, HUDs) —
     * same contract as GetKeyboardEmulationKeys.
     */
    static const TArray<FPartyGamepadMapping>& GetGamepadFaceButtonMappings();

protected:
    /**
     * When true (default), BuildButtonInputs also maps the keyboard emulation
     * keys (asdfjkl;ertghuio) to the same 16 actions, alongside the USB HID
     * controller keys. Set false for controller-only (production).
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    bool bEnableKeyboardEmulation = true;

    /**
     * When true (default), BuildButtonInputs also maps the four gamepad face
     * buttons to the player buttons in the matching screen direction (see
     * GetGamepadFaceButtonMappings). A separate flag from the keyboard scheme
     * because it is a genuinely different input device, not a key layout — and
     * because a gamepad plugged in alongside the cabinet should be an opt-out.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    bool bEnableGamepadEmulation = true;

    /**
     * How long the main button must be held (in seconds) to fire OnMainButtonHeld
     * instead of OnMainButtonTapped. Configurable per-blueprint-subclass.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    float MainButtonHoldThreshold = 0.6f;

    /**
     * When true (default), BuildButtonInputs also maps a generic dev-only
     * increment/decrement control to the Up/Down arrow keys (OnDevIncrement /
     * OnDevDecrement). These have no physical arcade-cabinet equivalent —
     * purely a development/testing convenience (e.g. adjusting an AI player
     * count in the Lobby). Kept separate from bEnableKeyboardEmulation since
     * arrow keys are not part of the asdfjkl;ertghuio button-emulation scheme.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    bool bEnableDevControls = true;

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    /**
     * Override-able in Blueprint or C++ subclasses to swap in asset-authored inputs.
     * Assign IMC_Buttons here, and fill ButtonActions with IA_Button01..16 IN ORDER.
     * Leave empty to use the runtime-built inputs (the default).
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputMappingContext> ButtonMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TArray<TObjectPtr<UInputAction>> ButtonActions;

    /**
     * The 17th "main button" action — host/MC control.
     * Kept SEPARATE from ButtonActions to preserve the 16-player invariant.
     * Built at runtime by BuildButtonInputs alongside the 16 player actions.
     * Physical: GenericUSBController_Button17. Dev: Enter.
     * Designer override: assign this property to skip runtime creation.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputAction> MainButtonAction;

    /** Generic dev-only "increment" action, mapped only to the Up arrow key. See bEnableDevControls. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputAction> DevIncrementAction;

    /** Generic dev-only "decrement" action, mapped only to the Down arrow key. See bEnableDevControls. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputAction> DevDecrementAction;

    /** Generic dev-only "left" action, mapped only to the Left arrow key. See bEnableDevControls. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputAction> DevLeftAction;

    /** Generic dev-only "right" action, mapped only to the Right arrow key. See bEnableDevControls. */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputAction> DevRightAction;

    /**
     * Dev menu action. Mapped to Tab and bound only in non-Shipping builds — the
     * property itself can't be wrapped in #if (UHT rejects a UPROPERTY inside a
     * preprocessor block), so in Shipping this is simply an action that nothing
     * ever maps a key to and nothing ever binds a handler to.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    TObjectPtr<UInputAction> DevMenuAction;

    /** Bound to all 16 actions' ETriggerEvent::Started. Resolves which action fired. */
    void OnAnyButton(const FInputActionInstance& Instance);

    /** Bound to all 16 actions' ETriggerEvent::Completed. Fires HandleButtonReleased. */
    void OnAnyButtonReleased(const FInputActionInstance& Instance);

    /** Bound to MainButtonAction Started/Completed to classify tap vs long-press. */
    void OnMainButtonStarted(const FInputActionInstance& Instance);
    void OnMainButtonCompleted(const FInputActionInstance& Instance);

    /** Fires at MainButtonHoldThreshold while the main button is still held. */
    void OnMainHoldTimerFired();

    /** Bound to the Dev*Action Started events. A press is a single nudge — no hold semantics. */
    void OnDevIncrementStarted(const FInputActionInstance& Instance);
    void OnDevDecrementStarted(const FInputActionInstance& Instance);
    void OnDevLeftStarted(const FInputActionInstance& Instance);
    void OnDevRightStarted(const FInputActionInstance& Instance);
#if !UE_BUILD_SHIPPING
    void OnDevMenuStarted(const FInputActionInstance& Instance);
#endif

    /**
     * ===== HANDOFF POINT =====
     * PlayerIndex 0 => physical button 1 / player 1, etc.
     * Override this in a subclass or bind OnButtonPressed to react to presses.
     */
    virtual void HandleButtonPressed(int32 PlayerIndex);
    virtual void HandleButtonReleased(int32 PlayerIndex);

    /** Override or bind OnMainButtonTapped / OnMainButtonHeld to react. */
    virtual void HandleMainButtonTapped();
    virtual void HandleMainButtonHeld();

    /** Override or bind OnDevIncrement / OnDevDecrement / OnDevLeft / OnDevRight / OnDevMenu to react. */
    virtual void HandleDevIncrement();
    virtual void HandleDevDecrement();
    virtual void HandleDevLeft();
    virtual void HandleDevRight();
#if !UE_BUILD_SHIPPING
    virtual void HandleDevMenu();
#endif

private:
    static constexpr int32 NUM_BUTTONS = 16;

    /** Timestamp (world seconds) when the main button was last pressed. */
    double MainButtonPressTime = 0.0;

    /** True once the hold timer fires, so Completed doesn't also send a tap. */
    bool bMainHoldFired = false;

    FTimerHandle MainHoldTimerHandle;
};
