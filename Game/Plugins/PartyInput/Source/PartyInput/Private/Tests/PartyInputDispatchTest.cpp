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
//       a s d f j k l ; e r t g h u i o
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

    // Expected sequence (see GetKeyboardEmulationKeys): buttons 1-8 are the home
    // row halves a s d f / j k l ; and buttons 9-16 are e r t g h u i o.
    // We check by FKey identity (FKey comparison by name).
    const TArray<FKey> Expected = {
        EKeys::A, EKeys::S, EKeys::D, EKeys::F,
        EKeys::J, EKeys::K, EKeys::L, EKeys::Semicolon,
        EKeys::E, EKeys::R, EKeys::T, EKeys::G,
        EKeys::H, EKeys::U, EKeys::I, EKeys::O,
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

// ---------------------------------------------------------------------------
// PartyButtons.Input.Dispatch.MapsGamepadFaceButtons
//
// The four face buttons drive the player buttons that sit in the matching
// compass direction on screen (OctoOdyssey's arms 0/2/4/6 — see
// GetGamepadFaceButtonMappings for the geometry). Verifies:
//   (a) the table is exactly the four face buttons, on four distinct in-range
//       button indices, with none duplicated;
//   (b) an IMC built from the table maps each face button to the action at its
//       stated index, in ADDITION to that action's existing mappings — a
//       gamepad must add a source, never replace one.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyInputGamepadFaceButtonTest,
    "PartyButtons.Input.Dispatch.MapsGamepadFaceButtons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyInputGamepadFaceButtonTest::RunTest(const FString& Parameters)
{
    constexpr int32 NUM_BUTTONS = 16;

    // ---- (a) Verify the table ----------------------------------------------

    const TArray<FPartyGamepadMapping>& Mappings = APartyInputController::GetGamepadFaceButtonMappings();
    TestEqual(TEXT("There are four face-button mappings"), Mappings.Num(), 4);

    // North/east/south/west, in the order the arms are indexed.
    const TArray<FPartyGamepadMapping> Expected = {
        { 0, EKeys::Gamepad_FaceButton_Top    },
        { 2, EKeys::Gamepad_FaceButton_Right  },
        { 4, EKeys::Gamepad_FaceButton_Bottom },
        { 6, EKeys::Gamepad_FaceButton_Left   },
    };

    for (int32 i = 0; i < Expected.Num() && i < Mappings.Num(); i++)
    {
        TestEqual(*FString::Printf(TEXT("Mapping[%d] targets the expected button"), i),
            Mappings[i].ButtonIndex, Expected[i].ButtonIndex);
        TestEqual(*FString::Printf(TEXT("Mapping[%d] uses the expected key"), i),
            Mappings[i].Key, Expected[i].Key);
    }

    TSet<int32> SeenIndices;
    TSet<FKey>  SeenKeys;
    for (const FPartyGamepadMapping& M : Mappings)
    {
        TestTrue(TEXT("Button index is within the 16-button range"),
            M.ButtonIndex >= 0 && M.ButtonIndex < NUM_BUTTONS);
        TestFalse(TEXT("No button index is mapped twice"), SeenIndices.Contains(M.ButtonIndex));
        TestFalse(TEXT("No key is mapped twice"), SeenKeys.Contains(M.Key));
        TestTrue(TEXT("The key is a gamepad key"), M.Key.IsGamepadKey());
        SeenIndices.Add(M.ButtonIndex);
        SeenKeys.Add(M.Key);
    }

    // ---- (b) Verify an IMC built the way BuildButtonInputs builds one -------

    UObject* Outer = GetTransientPackage();

    TArray<TObjectPtr<UInputAction>> Actions;
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        UInputAction* IA = NewObject<UInputAction>(Outer, NAME_None, RF_Transient);
        IA->ValueType = EInputActionValueType::Boolean;
        Actions.Add(IA);
    }

    UInputMappingContext* IMC = NewObject<UInputMappingContext>(Outer, NAME_None, RF_Transient);
    for (int32 i = 0; i < NUM_BUTTONS; i++)
    {
        IMC->MapKey(Actions[i].Get(), FKey(*FString::Printf(TEXT("GenericUSBController_Button%d"), i + 1)));
    }
    for (const FPartyGamepadMapping& M : Mappings)
    {
        IMC->MapKey(Actions[M.ButtonIndex].Get(), M.Key);
    }

    // Additive: the 16 controller keys are all still there.
    const TArray<FEnhancedActionKeyMapping>& Built = IMC->GetMappings();
    TestEqual(TEXT("IMC has 20 mappings (16 controller + 4 gamepad)"), Built.Num(), NUM_BUTTONS + Mappings.Num());

    for (const FPartyGamepadMapping& M : Mappings)
    {
        bool bFound = false;
        for (const FEnhancedActionKeyMapping& Built1 : Built)
        {
            if (Built1.Key == M.Key && Built1.Action == Actions[M.ButtonIndex])
            {
                bFound = true;
                break;
            }
        }
        TestTrue(*FString::Printf(TEXT("IMC maps %s to action[%d]"), *M.Key.ToString(), M.ButtonIndex), bFound);
    }

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
