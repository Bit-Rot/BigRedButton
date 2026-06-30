#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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
    /** Broadcast every time a button is pressed. PlayerIndex is 0-based. */
    FOnPartyButtonPressed OnButtonPressed;

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
     * Order: a w s e d r f g h j i k o l p ;
     * Used by BuildButtonInputs and the demo HUD. Static so it can be called
     * without a controller instance (e.g., in tests or the HUD).
     */
    static const TArray<FKey>& GetKeyboardEmulationKeys();

protected:
    /**
     * When true (default), BuildButtonInputs also maps the keyboard emulation
     * keys (awsedrfghjikolp;) to the same 16 actions, alongside the USB HID
     * controller keys. Set false for controller-only (production).
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyInput")
    bool bEnableKeyboardEmulation = true;
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

    /** Bound to all 16 actions' ETriggerEvent::Started. Resolves which action fired. */
    void OnAnyButton(const FInputActionInstance& Instance);

    /**
     * ===== HANDOFF POINT =====
     * PlayerIndex 0 => physical button 1 / player 1, etc.
     * Override this in a subclass or bind OnButtonPressed to react to presses.
     */
    virtual void HandleButtonPressed(int32 PlayerIndex);

private:
    static constexpr int32 NUM_BUTTONS = 16;
};
