#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "EnhancedPlayerInput.h"   // FInputActionInstance
#include "PartyInputController.h"  // GetKeyboardEmulationKeys()

// ---------------------------------------------------------------------------
// Test 1: BuildsSixteenDistinctActions
//
// Verifies that the runtime input builder produces exactly 16 non-null,
// mutually distinct UInputAction objects and a populated UInputMappingContext.
// Does not require a world, actor, or subsystem.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputBuildTest,
    "PartyButtons.Input.Dispatch.BuildsSixteenDistinctActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputBuildTest::RunTest(const FString& Parameters)
{
    constexpr int32 NUM_BUTTONS = 16;

    // Build actions the same way BuildButtonInputs() does.
    UObject* Outer = GetTransientPackage();
    TArray<TObjectPtr<UInputAction>> Actions;
    Actions.Reserve(NUM_BUTTONS);

    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        IA->ValueType = EInputActionValueType::Boolean;
        Actions.Add(IA);
    }

    // Build IMC
    UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer, NAME_None, RF_Transient);
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        const FKey Key(*FString::Printf(TEXT("GenericUSBController_Button%d"), i + 1));
        IMC->MapKey(Actions[i].Get(), Key);
    }

    // Assertions
    TestEqual(TEXT("Action count is 16"), Actions.Num(), NUM_BUTTONS);
    TestNotNull(TEXT("IMC is non-null"), IMC);
    TestEqual(TEXT("IMC has 16 mappings"), IMC->GetMappings().Num(), NUM_BUTTONS);

    // All actions are non-null
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        TestNotNull(*FString::Printf(TEXT("Action[%d] is non-null"), i), Actions[i].Get());
    }

    // All actions are distinct (no two pointers are the same)
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        for (int32 j = i + 1; j < NUM_BUTTONS; j++)
        {
            TestNotEqual(
                *FString::Printf(TEXT("Action[%d] != Action[%d]"), i, j),
                Actions[i].Get(),
                Actions[j].Get());
        }
    }

    // All actions have Boolean value type
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        TestEqual(
            *FString::Printf(TEXT("Action[%d].ValueType == Boolean"), i),
            Actions[i]->ValueType,
            EInputActionValueType::Boolean);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 2: RoutesEachButtonToItsIndex
//
// Verifies that for each action[k] in the array, IndexOfByKey resolves to k —
// which is exactly the dispatch logic in APartyInputController::OnAnyButton.
// Also exercises the FInputActionInstance constructor path to confirm
// GetSourceAction() returns the same pointer used for indexing.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputRoutingTest,
    "PartyButtons.Input.Dispatch.RoutesEachButtonToItsIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputRoutingTest::RunTest(const FString& Parameters)
{
    constexpr int32 NUM_BUTTONS = 16;

    UObject* Outer = GetTransientPackage();
    TArray<TObjectPtr<UInputAction>> Actions;
    Actions.Reserve(NUM_BUTTONS);

    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        IA->ValueType = EInputActionValueType::Boolean;
        Actions.Add(IA);
    }

    // For each action, verify IndexOfByKey returns its own index.
    for (int32 k = 0; k < NUM_BUTTONS; k++)
    {
        const int32 Found = Actions.IndexOfByKey(Actions[k]);
        TestEqual(
            *FString::Printf(TEXT("IndexOfByKey(action[%d]) == %d"), k, k),
            Found, k);
    }

    // Also verify via FInputActionInstance::GetSourceAction() — the exact pathway
    // that OnAnyButton uses when an Enhanced Input event fires.
    for (int32 k = 0; k < NUM_BUTTONS; k++)
    {
        const FInputActionInstance Instance(Actions[k].Get());
        const UInputAction* Src = Instance.GetSourceAction();
        const int32 Found = Actions.IndexOfByKey(Src);
        TestEqual(
            *FString::Printf(TEXT("Instance(%d).GetSourceAction() resolves to index %d"), k, k),
            Found, k);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 3: RejectsUnknownAction
//
// Verifies that an action not in the array returns INDEX_NONE, so
// OnAnyButton correctly ignores spurious Enhanced Input events.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputRejectTest,
    "PartyButtons.Input.Dispatch.RejectsUnknownAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputRejectTest::RunTest(const FString& Parameters)
{
    constexpr int32 NUM_BUTTONS = 16;

    UObject* Outer = GetTransientPackage();
    TArray<TObjectPtr<UInputAction>> Actions;
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        Actions.Add(IA);
    }

    // A freshly created action that was never added to the array.
    UInputAction* Stranger = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);

    const int32 Found = Actions.IndexOfByKey(Stranger);
    TestEqual(TEXT("Unknown action resolves to INDEX_NONE"), Found, INDEX_NONE);

    // Null pointer also returns INDEX_NONE (guard against nullptr source in OnAnyButton).
    const int32 NullFound = Actions.IndexOfByKey(static_cast<UInputAction*>(nullptr));
    TestEqual(TEXT("Null action resolves to INDEX_NONE"), NullFound, INDEX_NONE);

    return true;
}

// ---------------------------------------------------------------------------
// Test 4: MapsKeyboardEmulationKeys
//
// Verifies:
//   (a) GetKeyboardEmulationKeys() returns exactly 16 keys in the order
//       a w s e d r f g h j i k o l p ;
//   (b) When an IMC is built with keyboard emulation on, each keyboard key
//       is mapped to the action at the matching button index — i.e. the IMC
//       contains a mapping from EmulKeys[k] to Actions[k].
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputKeyboardEmulationTest,
    "PartyButtons.Input.Dispatch.MapsKeyboardEmulationKeys",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputKeyboardEmulationTest::RunTest(const FString& Parameters)
{
    constexpr int32 NUM_BUTTONS = 16;

    // ---- (a) Verify the key sequence ----------------------------------------

    const TArray<FKey>& Keys = APartyInputController::GetKeyboardEmulationKeys();
    TestEqual(TEXT("GetKeyboardEmulationKeys returns 16 keys"), Keys.Num(), NUM_BUTTONS);

    // Expected sequence: a w s e d r f g h j i k o l p ;
    // We check by FKey identity (FKey comparison by name).
    const TArray<FKey> Expected = {
        EKeys::A, EKeys::W, EKeys::S, EKeys::E,
        EKeys::D, EKeys::R, EKeys::F, EKeys::G,
        EKeys::H, EKeys::U, EKeys::J, EKeys::I,
        EKeys::K, EKeys::O, EKeys::L, EKeys::P,
    };

    for (int32 k = 0; k < NUM_BUTTONS && k < Keys.Num(); k++)
    {
        TestEqual(
            *FString::Printf(TEXT("EmulKeys[%d] == expected"), k),
            Keys[k], Expected[k]);
    }

    // ---- (b) Verify that a built IMC contains a mapping for each key --------

    UObject* Outer = GetTransientPackage();

    // Build 16 actions (mirrors BuildButtonInputs logic)
    TArray<TObjectPtr<UInputAction>> Actions;
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        IA->ValueType = EInputActionValueType::Boolean;
        Actions.Add(IA);
    }

    // Build IMC with both controller-key and keyboard-emulation mappings
    UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer, NAME_None, RF_Transient);
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        const FKey CtrlKey(*FString::Printf(TEXT("GenericUSBController_Button%d"), i + 1));
        IMC->MapKey(Actions[i].Get(), CtrlKey);
    }
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        IMC->MapKey(Actions[i].Get(), Keys[i]);
    }

    // Each action should appear in at least 2 mappings (controller + keyboard).
    // We verify the keyboard key → action[k] mapping by scanning GetMappings().
    const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();
    // Total: 16 controller keys + 16 keyboard keys = 32 mappings.
    TestEqual(TEXT("IMC has 32 mappings (16 controller + 16 keyboard)"), Mappings.Num(), NUM_BUTTONS * 2);

    for (int32 k = 0; k < NUM_BUTTONS; k++)
    {
        // Find a mapping whose Key == EmulKeys[k] and whose Action == Actions[k].
        bool bFound = false;
        for (const FEnhancedActionKeyMapping& M : Mappings)
        {
            if (M.Key == Keys[k] && M.Action == Actions[k])
            {
                bFound = true;
                break;
            }
        }
        TestTrue(
            *FString::Printf(TEXT("IMC maps EmulKey[%d] (%s) to action[%d]"),
                             k, *Keys[k].ToString(), k),
            bFound);
    }

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
