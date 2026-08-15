#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "OctoOdyssey/OctoTuning.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Tuning.*
//
// Tests for the tuning descriptor table and its pure helpers. No world, no
// actor — the table and the JSON writer are exactly the kind of thing that
// silently rots (a field added without a row, a default nudged outside its
// slider range), so this is where that gets caught.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningTableIsWellFormed,
    "PartyButtons.Octo.Tuning.TableIsWellFormed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningTableIsWellFormed::RunTest(const FString& Parameters)
{
    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();

    TestTrue(TEXT("The table is not empty"), Params.Num() > 0);

    TSet<FString> SeenNames;

    for (const FOctoTuningParam& P : Params)
    {
        const FString Name = P.Name ? FString(P.Name) : FString();

        TestTrue(TEXT("Every param has a name"), !Name.IsEmpty());
        TestTrue(FString::Printf(TEXT("Every param has a category (%s)"), *Name),
            P.Category != nullptr && FCString::Strlen(P.Category) > 0);

        // Duplicate names would collide as JSON keys and be indistinguishable in the menu.
        TestFalse(FString::Printf(TEXT("Param name is unique (%s)"), *Name), SeenNames.Contains(Name));
        SeenNames.Add(Name);

        TestTrue(FString::Printf(TEXT("Min < Max (%s)"), *Name), P.Min < P.Max);
        TestTrue(FString::Printf(TEXT("Step > 0 (%s)"), *Name), P.Step > 0.f);

        // Exactly one member pointer, matching Kind — a mismatch would silently
        // make the row a no-op in the menu.
        if (P.Kind == EOctoParamKind::Float)
        {
            TestTrue(FString::Printf(TEXT("Float param has a float member (%s)"), *Name), P.FloatMember != nullptr);
            TestTrue(FString::Printf(TEXT("Float param has no bool member (%s)"), *Name), P.BoolMember == nullptr);
        }
        else
        {
            TestTrue(FString::Printf(TEXT("Bool param has a bool member (%s)"), *Name), P.BoolMember != nullptr);
            TestTrue(FString::Printf(TEXT("Bool param has no float member (%s)"), *Name), P.FloatMember == nullptr);
        }
    }

    // Rows are grouped into sections by category in menu order, so a category
    // must not reappear after another one has started.
    TSet<FString> ClosedCategories;
    FString CurrentCategory;
    for (const FOctoTuningParam& P : Params)
    {
        const FString Category(P.Category);
        if (Category != CurrentCategory)
        {
            TestFalse(FString::Printf(TEXT("Category %s is contiguous"), *Category),
                ClosedCategories.Contains(Category));
            if (!CurrentCategory.IsEmpty())
            {
                ClosedCategories.Add(CurrentCategory);
            }
            CurrentCategory = Category;
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningDefaultsAreInRange,
    "PartyButtons.Octo.Tuning.DefaultsAreInRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningDefaultsAreInRange::RunTest(const FString& Parameters)
{
    // If a shipping default sits outside its slider's range, the menu would jump
    // the value the instant it was touched — losing the very number that shipped.
    const FOctoTuning Defaults;

    for (const FOctoTuningParam& P : OctoTuning::GetParams())
    {
        if (P.Kind != EOctoParamKind::Float || !P.FloatMember) { continue; }

        const float Value = Defaults.*(P.FloatMember);
        TestTrue(
            FString::Printf(TEXT("Default %s (%.3f) is within [%.3f, %.3f]"), P.Name, Value, P.Min, P.Max),
            Value >= P.Min && Value <= P.Max);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningNormalizedRoundTrips,
    "PartyButtons.Octo.Tuning.NormalizedRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningNormalizedRoundTrips::RunTest(const FString& Parameters)
{
    for (const FOctoTuningParam& P : OctoTuning::GetParams())
    {
        if (P.Kind != EOctoParamKind::Float) { continue; }

        FOctoTuning Tuning;

        // The ends must be exact: a slider dragged fully left has to produce Min,
        // not Min plus a rounding crumb.
        OctoTuning::SetNormalized(Tuning, P, 0.f);
        TestEqual(FString::Printf(TEXT("%s at alpha 0 is Min"), P.Name), Tuning.*(P.FloatMember), P.Min);
        TestEqual(FString::Printf(TEXT("%s reads back as 0"), P.Name), OctoTuning::GetNormalized(Tuning, P), 0.f);

        OctoTuning::SetNormalized(Tuning, P, 1.f);
        TestEqual(FString::Printf(TEXT("%s at alpha 1 is Max"), P.Name), Tuning.*(P.FloatMember), P.Max);
        TestEqual(FString::Printf(TEXT("%s reads back as 1"), P.Name), OctoTuning::GetNormalized(Tuning, P), 1.f);

        // Out-of-range alphas clamp rather than escaping the slider.
        OctoTuning::SetNormalized(Tuning, P, -5.f);
        TestEqual(FString::Printf(TEXT("%s clamps alpha below 0"), P.Name), Tuning.*(P.FloatMember), P.Min);

        OctoTuning::SetNormalized(Tuning, P, 5.f);
        TestEqual(FString::Printf(TEXT("%s clamps alpha above 1"), P.Name), Tuning.*(P.FloatMember), P.Max);

        // Mid-drag values snap to Step but must stay in range and round-trip to
        // roughly where they were put.
        OctoTuning::SetNormalized(Tuning, P, 0.5f);
        const float Mid = Tuning.*(P.FloatMember);
        TestTrue(FString::Printf(TEXT("%s at alpha 0.5 stays in range"), P.Name), Mid >= P.Min && Mid <= P.Max);

        const float StepAlpha = P.Step / (P.Max - P.Min);
        TestTrue(FString::Printf(TEXT("%s at alpha 0.5 reads back within one step"), P.Name),
            FMath::Abs(OctoTuning::GetNormalized(Tuning, P) - 0.5f) <= StepAlpha);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningNudgeRespectsBounds,
    "PartyButtons.Octo.Tuning.NudgeRespectsBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningNudgeRespectsBounds::RunTest(const FString& Parameters)
{
    for (const FOctoTuningParam& P : OctoTuning::GetParams())
    {
        FOctoTuning Tuning;

        if (P.Kind == EOctoParamKind::Bool)
        {
            // Direction, not toggle: holding a key must not flicker the value.
            OctoTuning::Nudge(Tuning, P, +1);
            TestTrue(FString::Printf(TEXT("%s right is On"), P.Name), Tuning.*(P.BoolMember));
            OctoTuning::Nudge(Tuning, P, +1);
            TestTrue(FString::Printf(TEXT("%s right again stays On"), P.Name), Tuning.*(P.BoolMember));

            OctoTuning::Nudge(Tuning, P, -1);
            TestFalse(FString::Printf(TEXT("%s left is Off"), P.Name), Tuning.*(P.BoolMember));
            continue;
        }

        // Walk off each end and confirm the value pins rather than escaping.
        OctoTuning::SetNormalized(Tuning, P, 1.f);
        OctoTuning::Nudge(Tuning, P, +1);
        TestEqual(FString::Printf(TEXT("%s nudging up at Max stays at Max"), P.Name), Tuning.*(P.FloatMember), P.Max);

        OctoTuning::SetNormalized(Tuning, P, 0.f);
        OctoTuning::Nudge(Tuning, P, -1);
        TestEqual(FString::Printf(TEXT("%s nudging down at Min stays at Min"), P.Name), Tuning.*(P.FloatMember), P.Min);

        // And that one nudge inward actually moves by exactly Step.
        OctoTuning::Nudge(Tuning, P, +1);
        TestTrue(FString::Printf(TEXT("%s nudging up from Min moves one step"), P.Name),
            FMath::IsNearlyEqual(Tuning.*(P.FloatMember), FMath::Min(P.Min + P.Step, P.Max), KINDA_SMALL_NUMBER));

        // Direction 0 is a documented no-op.
        const float Before = Tuning.*(P.FloatMember);
        OctoTuning::Nudge(Tuning, P, 0);
        TestEqual(FString::Printf(TEXT("%s nudging by 0 changes nothing"), P.Name), Tuning.*(P.FloatMember), Before);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningJsonContainsEveryParam,
    "PartyButtons.Octo.Tuning.JsonContainsEveryParam",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningJsonContainsEveryParam::RunTest(const FString& Parameters)
{
    const FOctoTuning Defaults;
    const FString Json = OctoTuning::ToJsonString(Defaults);

    TestTrue(TEXT("JSON opens with a brace"), Json.StartsWith(TEXT("{")));
    TestTrue(TEXT("JSON closes with a brace"), Json.EndsWith(TEXT("}")));

    TArrayView<const FOctoTuningParam> Params = OctoTuning::GetParams();

    for (const FOctoTuningParam& P : Params)
    {
        TestTrue(FString::Printf(TEXT("JSON contains a key for %s"), P.Name),
            Json.Contains(FString::Printf(TEXT("\"%s\":"), P.Name)));
    }

    // One separator fewer than there are keys — catches a stray or missing comma.
    int32 CommaCount = 0;
    for (TCHAR C : Json)
    {
        if (C == TEXT(',')) { CommaCount++; }
    }
    TestEqual(TEXT("JSON has one comma per key gap"), CommaCount, Params.Num() - 1);

    // Spot-check both kinds against the shipping defaults so a broken formatter
    // can't pass on structure alone.
    TestTrue(TEXT("JSON carries the default ExtendSpeed"), Json.Contains(TEXT("\"ExtendSpeed\":1200.0")));
    TestTrue(TEXT("JSON carries the default bPushAtImpactPoint"), Json.Contains(TEXT("\"bPushAtImpactPoint\":true")));
    TestTrue(TEXT("JSON carries the default bSweepPhysicsBodies"), Json.Contains(TEXT("\"bSweepPhysicsBodies\":false")));

    return true;
}

// --------------------------------------------------------------------------
// Persistence. These go all the way to a real file on disk on purpose.
//
// The first implementation of this used GConfig->SetFloat + Flush, which
// silently wrote NOTHING (no cached FConfigFile for a filename that had never
// been loaded) while reporting success — and a test that only exercised the
// read path against a hand-written ini passed happily throughout. Anything less
// than an actual save-then-load round trip would not have caught it.
// --------------------------------------------------------------------------

namespace
{
    /** A scratch ini path unique to the running test, cleaned up by the caller. */
    FString MakeTuningScratchIniPath(const TCHAR* Tag)
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::AutomationTransientDir(),
                FString::Printf(TEXT("OctoTuningTest_%s_%s.ini"), Tag, *FGuid::NewGuid().ToString())));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningIniRoundTrips,
    "PartyButtons.Octo.Tuning.IniRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningIniRoundTrips::RunTest(const FString& Parameters)
{
    const FString IniPath = MakeTuningScratchIniPath(TEXT("RoundTrip"));
    IFileManager::Get().Delete(*IniPath);

    // ---- Save ------------------------------------------------------------

    FOctoTuning Saved;
    Saved.ExtendSpeed         = 1250.f;
    Saved.ArmGripFraction     = 0.45f;
    Saved.SphereRadius        = 65.f;
    Saved.SurfaceFriction     = 1.3f;
    Saved.bSweepPhysicsBodies = true;   // flipped from its default
    Saved.bPushAtImpactPoint  = false;  // flipped from its default

    TestTrue(TEXT("SaveToIni reports success"), OctoTuning::SaveToIni(Saved, IniPath));

    // The bug this test exists for: "success" with no file.
    TestTrue(TEXT("The ini actually exists on disk"), IFileManager::Get().FileExists(*IniPath));

    FString FileContents;
    TestTrue(TEXT("The ini is readable"), FFileHelper::LoadFileToString(FileContents, *IniPath));
    TestTrue(TEXT("The ini has the expected section"),
        FileContents.Contains(FString::Printf(TEXT("[%s]"), OctoTuning::IniSection)));
    TestTrue(TEXT("The ini is not empty"), FileContents.TrimStartAndEnd().Len() > 0);

    // ---- Load ------------------------------------------------------------

    FOctoTuning Loaded; // starts at the defaults, as the subsystem does
    FString Overridden;
    const int32 NumOverridden = OctoTuning::LoadFromIni(Loaded, IniPath, &Overridden);

    TestEqual(TEXT("Every param round-trips"), NumOverridden, OctoTuning::GetParams().Num());

    TestEqual(TEXT("ExtendSpeed round-trips"), Loaded.ExtendSpeed, Saved.ExtendSpeed);
    TestEqual(TEXT("ArmGripFraction round-trips"), Loaded.ArmGripFraction, Saved.ArmGripFraction);
    TestEqual(TEXT("SphereRadius round-trips"), Loaded.SphereRadius, Saved.SphereRadius);
    TestEqual(TEXT("SurfaceFriction round-trips"), Loaded.SurfaceFriction, Saved.SurfaceFriction);
    TestEqual(TEXT("bSweepPhysicsBodies round-trips"), Loaded.bSweepPhysicsBodies, Saved.bSweepPhysicsBodies);
    TestEqual(TEXT("bPushAtImpactPoint round-trips"), Loaded.bPushAtImpactPoint, Saved.bPushAtImpactPoint);
    TestTrue(TEXT("The override log names a value"), Overridden.Contains(TEXT("ExtendSpeed")));

    // Every float field, generically — catches a param that saves but doesn't load.
    for (const FOctoTuningParam& P : OctoTuning::GetParams())
    {
        if (P.Kind == EOctoParamKind::Float && P.FloatMember)
        {
            TestEqual(*FString::Printf(TEXT("%s round-trips"), P.Name),
                Loaded.*(P.FloatMember), Saved.*(P.FloatMember));
        }
        else if (P.BoolMember)
        {
            TestEqual(*FString::Printf(TEXT("%s round-trips"), P.Name),
                Loaded.*(P.BoolMember), Saved.*(P.BoolMember));
        }
    }

    IFileManager::Get().Delete(*IniPath);
    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoTuningIniOverlaysDefaults,
    "PartyButtons.Octo.Tuning.IniOverlaysDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoTuningIniOverlaysDefaults::RunTest(const FString& Parameters)
{
    const FString IniPath = MakeTuningScratchIniPath(TEXT("Overlay"));
    IFileManager::Get().Delete(*IniPath);

    // ---- A missing file changes nothing ----------------------------------

    FOctoTuning Tuning;
    TestEqual(TEXT("A missing ini overrides nothing"), OctoTuning::LoadFromIni(Tuning, IniPath), 0);
    TestEqual(TEXT("A missing ini leaves ExtendSpeed at its default"),
        Tuning.ExtendSpeed, FOctoTuning().ExtendSpeed);

    // ---- A partial, hand-written file overlays only what it mentions ------

    const FString HandWritten = FString::Printf(
        TEXT("[%s]\r\nExtendSpeed=1500.000000\r\nSphereRadius=99999.000000\r\nbSweepPhysicsBodies=True\r\n"),
        OctoTuning::IniSection);
    TestTrue(TEXT("Scratch ini written"), FFileHelper::SaveStringToFile(HandWritten, *IniPath));

    FOctoTuning Partial;
    const int32 NumOverridden = OctoTuning::LoadFromIni(Partial, IniPath);

    TestEqual(TEXT("Only the three named fields are overridden"), NumOverridden, 3);
    TestEqual(TEXT("ExtendSpeed comes from the file"), Partial.ExtendSpeed, 1500.f);
    TestTrue(TEXT("bSweepPhysicsBodies comes from the file"), Partial.bSweepPhysicsBodies);

    // Out-of-range values clamp rather than escaping the sliders — a hand-edited
    // file is the easiest way to produce one.
    TestEqual(TEXT("An out-of-range SphereRadius clamps to its Max"), Partial.SphereRadius, 150.f);

    // Unmentioned fields keep their C++ default, which is what makes adding a
    // new tunable safe without a migration.
    TestEqual(TEXT("An unmentioned field keeps its default"),
        Partial.RetractSpeed, FOctoTuning().RetractSpeed);
    TestEqual(TEXT("An unmentioned bool keeps its default"),
        Partial.bPushAtImpactPoint, FOctoTuning().bPushAtImpactPoint);

    // ---- Saving over an existing file replaces those values --------------

    FOctoTuning Resaved;
    Resaved.ExtendSpeed = 300.f;
    TestTrue(TEXT("Re-saving over an existing ini succeeds"), OctoTuning::SaveToIni(Resaved, IniPath));

    FOctoTuning Reloaded;
    OctoTuning::LoadFromIni(Reloaded, IniPath);
    TestEqual(TEXT("The re-saved value wins"), Reloaded.ExtendSpeed, 300.f);
    TestEqual(TEXT("A field reset to default is rewritten too"),
        Reloaded.SphereRadius, FOctoTuning().SphereRadius);

    IFileManager::Get().Delete(*IniPath);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
