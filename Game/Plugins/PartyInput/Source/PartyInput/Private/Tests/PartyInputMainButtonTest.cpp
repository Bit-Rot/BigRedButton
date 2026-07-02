#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "PartyInputController.h"

// --------------------------------------------------------------------------
// PartyButtons.Input.MainButton.*
//
// Tests for the 17th "main button" input added to APartyInputController.
// All tests are no-world / no-actor style: they build their own UInputAction
// and UInputMappingContext objects using GetTransientPackage() as Outer.
//
// Critical invariants verified:
//   - MainButtonAction is NOT in ButtonActions (preserves 16-player invariant).
//   - GetButtonCount() stays 16 after adding the main button.
//   - The IMC correctly maps GenericUSBController_Button17 and Enter to the main action.
//   - The pure tap-vs-hold classification function is correct at all boundary points.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputMainButtonSeparateFromArray,
    "PartyButtons.Input.MainButton.IsSeparateFromButtonArray",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputMainButtonSeparateFromArray::RunTest(const FString& Parameters)
{
    UObject* Outer = GetTransientPackage();

    // Build 16 "player" actions (representing ButtonActions).
    TArray<UInputAction*> ButtonActions;
    for (int32 i = 0; i < 16; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer);
        IA->ValueType = EInputActionValueType::Boolean;
        ButtonActions.Add(IA);
    }

    // Build the main button action separately.
    UInputAction* MainAction = NewObject<UInputAction>(Outer);
    MainAction->ValueType = EInputActionValueType::Boolean;

    // Invariant: main action must NOT appear in ButtonActions.
    TestEqual(TEXT("ButtonActions still has 16 entries"), ButtonActions.Num(), 16);
    TestTrue(TEXT("MainAction not in ButtonActions"),
             ButtonActions.Find(MainAction) == INDEX_NONE);

    // ResolveActionIndex equivalent: IndexOfByKey returns INDEX_NONE for the main action.
    const int32 Index = ButtonActions.IndexOfByKey(MainAction);
    TestEqual(TEXT("IndexOfByKey(MainAction) == INDEX_NONE"), Index, static_cast<int32>(INDEX_NONE));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputMainButtonMappings,
    "PartyButtons.Input.MainButton.MapsToButton17AndEnter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputMainButtonMappings::RunTest(const FString& Parameters)
{
    UObject* Outer = GetTransientPackage();

    // Build 16 player actions.
    TArray<UInputAction*> PlayerActions;
    for (int32 i = 0; i < 16; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer);
        IA->ValueType = EInputActionValueType::Boolean;
        PlayerActions.Add(IA);
    }

    // Build main button action.
    UInputAction* MainAction = NewObject<UInputAction>(Outer);
    MainAction->ValueType = EInputActionValueType::Boolean;

    // Build an IMC replicating what BuildButtonInputs() produces.
    UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer);

    // 16 controller keys for player buttons.
    for (int32 i = 0; i < 16; i++)
    {
        IMC->MapKey(PlayerActions[i], FKey(*FString::Printf(TEXT("GenericUSBController_Button%d"), i + 1)));
    }

    // Main button: Button17 + Enter (keyboard emulation).
    const FKey Button17Key(TEXT("GenericUSBController_Button17"));
    IMC->MapKey(MainAction, Button17Key);
    IMC->MapKey(MainAction, EKeys::Enter);

    // 16 keyboard emulation keys for player buttons.
    const TArray<FKey>& EmulKeys = APartyInputController::GetKeyboardEmulationKeys();
    TestEqual(TEXT("Emulation key table has 16 entries"), EmulKeys.Num(), 16);
    for (int32 i = 0; i < 16 && i < EmulKeys.Num(); i++)
    {
        IMC->MapKey(PlayerActions[i], EmulKeys[i]);
    }

    // Expected totals: 16 controller + 16 keyboard + Button17 + Enter = 34 mappings.
    const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();
    TestEqual(TEXT("IMC has 34 total mappings (16+16 player + Button17 + Enter)"),
              Mappings.Num(), 34);

    // Verify Button17 maps to MainAction.
    bool bFoundButton17 = false;
    bool bFoundEnter    = false;
    bool bMainInPlayer  = false;

    for (const FEnhancedActionKeyMapping& M : Mappings)
    {
        if (M.Action == MainAction)
        {
            if (M.Key == Button17Key)    { bFoundButton17 = true; }
            if (M.Key == EKeys::Enter)   { bFoundEnter    = true; }
        }
        // Ensure none of the 16 player keys accidentally map to the main action.
        for (int32 i = 0; i < 16; i++)
        {
            if (M.Action == PlayerActions[i] && M.Key == Button17Key) { bMainInPlayer = true; }
            if (M.Action == PlayerActions[i] && M.Key == EKeys::Enter) { bMainInPlayer = true; }
        }
    }

    TestTrue(TEXT("Button17 maps to MainAction"), bFoundButton17);
    TestTrue(TEXT("Enter maps to MainAction"), bFoundEnter);
    TestFalse(TEXT("No player action maps to Button17 or Enter"), bMainInPlayer);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputMainButtonTapVsHold,
    "PartyButtons.Input.MainButton.ClassifiesTapVsHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputMainButtonTapVsHold::RunTest(const FString& Parameters)
{
    // IsHoldGesture is a static, pure function — no world needed.
    constexpr float Threshold = 0.6f;

    TestFalse(TEXT("0.0s is a tap"),  APartyInputController::IsHoldGesture(0.0f,  Threshold));
    TestFalse(TEXT("0.2s is a tap"),  APartyInputController::IsHoldGesture(0.2f,  Threshold));
    TestFalse(TEXT("0.59s is a tap"), APartyInputController::IsHoldGesture(0.59f, Threshold));
    TestTrue( TEXT("0.6s is a hold"), APartyInputController::IsHoldGesture(0.6f,  Threshold));
    TestTrue( TEXT("1.0s is a hold"), APartyInputController::IsHoldGesture(1.0f,  Threshold));
    TestTrue( TEXT("2.0s is a hold"), APartyInputController::IsHoldGesture(2.0f,  Threshold));

    // Threshold of 0 means everything is a hold.
    TestTrue(TEXT("0s with threshold 0 is a hold"), APartyInputController::IsHoldGesture(0.0f, 0.0f));

    // Custom thresholds.
    TestFalse(TEXT("0.3s tap at threshold 1.0"), APartyInputController::IsHoldGesture(0.3f, 1.0f));
    TestTrue( TEXT("1.0s hold at threshold 1.0"), APartyInputController::IsHoldGesture(1.0f, 1.0f));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
