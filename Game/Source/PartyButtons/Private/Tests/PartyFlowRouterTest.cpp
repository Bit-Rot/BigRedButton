#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PartyFlowRouter.h"
#include "PartyTypes.h"

// --------------------------------------------------------------------------
// PartyButtons.Flow.Router.*
//
// Tests for the central PartyFlow::GetRoute() routing table.
// Pure code — no world, no actor, no engine subsystem.
//
// Critical invariants:
//   - Every phase returns a non-empty GameModeClassPath beginning "/Script/PartyButtons.".
//   - The path does NOT contain the 'A' prefix (e.g. "APartyLobbyGameMode" is WRONG;
//     the reflected name is "PartyLobbyGameMode" — this is the easy pitfall to miss).
//   - Fixed-phase MapNames start with "L_".
//   - Minigame phase MapName is NAME_None (caller fills it from the roster).
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyFlowRouterEveryPhase,
    "PartyButtons.Flow.Router.EveryFixedPhaseRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyFlowRouterEveryPhase::RunTest(const FString& Parameters)
{
    // Fixed phases: every one of these must have a non-None MapName with L_ prefix.
    const TArray<EPartyPhase> FixedPhases = {
        EPartyPhase::Main,
        EPartyPhase::MainMenu,
        EPartyPhase::Settings,
        EPartyPhase::Lobby,
        EPartyPhase::LevelSelect,
        EPartyPhase::Results,
    };

    for (EPartyPhase Phase : FixedPhases)
    {
        const FPartyPhaseRoute Route = PartyFlow::GetRoute(Phase);

        // MapName must be set (not None).
        TestFalse(
            FString::Printf(TEXT("Phase %d: MapName is not None"), static_cast<int32>(Phase)),
            Route.MapName.IsNone());

        // MapName must start with L_.
        TestTrue(
            FString::Printf(TEXT("Phase %d: MapName starts with L_"), static_cast<int32>(Phase)),
            Route.MapName.ToString().StartsWith(TEXT("L_")));

        // GameModeClassPath must be non-empty.
        TestFalse(
            FString::Printf(TEXT("Phase %d: GameModeClassPath is not empty"), static_cast<int32>(Phase)),
            Route.GameModeClassPath.IsEmpty());

        // GameModeClassPath must begin with "/Script/PartyButtons.".
        TestTrue(
            FString::Printf(TEXT("Phase %d: ClassPath begins /Script/PartyButtons."), static_cast<int32>(Phase)),
            Route.GameModeClassPath.StartsWith(TEXT("/Script/PartyButtons.")));

        // Critical pitfall guard: class name must NOT contain the 'A' prefix.
        // e.g. ".APartyLobbyGameMode" is wrong; ".PartyLobbyGameMode" is correct.
        const int32 DotIndex = Route.GameModeClassPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (DotIndex != INDEX_NONE)
        {
            const FString ClassName = Route.GameModeClassPath.Mid(DotIndex + 1);
            TestFalse(
                FString::Printf(TEXT("Phase %d: class name '%s' must not start with 'A' (UE script path pitfall)"),
                    static_cast<int32>(Phase), *ClassName),
                ClassName.StartsWith(TEXT("A")));
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyFlowRouterMinigame,
    "PartyButtons.Flow.Router.MinigameUsesSharedGameMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyFlowRouterMinigame::RunTest(const FString& Parameters)
{
    const FPartyPhaseRoute Route = PartyFlow::GetRoute(EPartyPhase::Minigame);

    // Minigame MapName is NAME_None — the caller fills it from the roster.
    TestTrue(TEXT("Minigame MapName is None"), Route.MapName.IsNone());

    // The shared GameMode path must be set correctly.
    TestFalse(TEXT("Minigame GameModeClassPath not empty"), Route.GameModeClassPath.IsEmpty());
    TestTrue( TEXT("Minigame GameModeClassPath starts /Script/PartyButtons."),
              Route.GameModeClassPath.StartsWith(TEXT("/Script/PartyButtons.")));

    // Must reference the shared minigame GameMode (not a per-map one).
    TestTrue(TEXT("Minigame uses PartyMinigameGameMode"),
             Route.GameModeClassPath.Contains(TEXT("MinigameGameMode")));

    // Pitfall: no 'A' prefix in class name.
    const int32 DotIndex = Route.GameModeClassPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (DotIndex != INDEX_NONE)
    {
        const FString ClassName = Route.GameModeClassPath.Mid(DotIndex + 1);
        TestFalse(TEXT("Minigame class name must not start with 'A'"),
                  ClassName.StartsWith(TEXT("A")));
    }

    // BuildGameModeOption must produce the correct ?game= string.
    const FString Option = PartyFlow::BuildGameModeOption(Route.GameModeClassPath);
    TestTrue(TEXT("BuildGameModeOption starts with ?game="), Option.StartsWith(TEXT("?game=")));
    TestTrue(TEXT("BuildGameModeOption contains the class path"),
             Option.Contains(Route.GameModeClassPath));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
