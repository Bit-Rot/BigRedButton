#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PartySessionState.h"
#include "Math/RandomStream.h"

// --------------------------------------------------------------------------
// PartyButtons.Session.State.*
// PartyButtons.Session.Roster.*
//
// Tests for FPartySessionState — the pure, world-free session data model.
// Stack-allocate the struct directly; no UObject, no world needed.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartySessionStateRegister,
    "PartyButtons.Session.State.RegisterPlayerDeduplicates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartySessionStateRegister::RunTest(const FString& Parameters)
{
    FPartySessionState S;

    // Initially nothing is registered.
    TestEqual(TEXT("Start: 0 registered"), S.GetRegisteredCount(), 0);
    TestFalse(TEXT("Start: player 0 not registered"), S.IsPlayerRegistered(0));

    // Register player 3.
    S.RegisterPlayer(3);
    TestTrue( TEXT("Player 3 registered"), S.IsPlayerRegistered(3));
    TestEqual(TEXT("Count = 1"), S.GetRegisteredCount(), 1);

    // Idempotent — register again.
    S.RegisterPlayer(3);
    TestEqual(TEXT("Count still 1 after duplicate"), S.GetRegisteredCount(), 1);

    // Out-of-range: silently ignored.
    S.RegisterPlayer(-1);
    S.RegisterPlayer(16);
    S.RegisterPlayer(100);
    TestEqual(TEXT("Out-of-range doesn't change count"), S.GetRegisteredCount(), 1);
    TestFalse(TEXT("IsPlayerRegistered(-1) == false"), S.IsPlayerRegistered(-1));
    TestFalse(TEXT("IsPlayerRegistered(16) == false"), S.IsPlayerRegistered(16));

    // Register all 16.
    for (int32 i = 0; i < 16; i++) { S.RegisterPlayer(i); }
    TestEqual(TEXT("All 16 registered"), S.GetRegisteredCount(), 16);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartySessionStateWins,
    "PartyButtons.Session.State.RecordWinIncrementsTally",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartySessionStateWins::RunTest(const FString& Parameters)
{
    FPartySessionState S;

    // GetLeader returns INDEX_NONE when nobody has won.
    TestEqual(TEXT("No leader before any wins"), S.GetLeader(), static_cast<int32>(INDEX_NONE));

    // Record some wins.
    S.RecordWin(2);
    S.RecordWin(2);
    S.RecordWin(5);

    // Leader is player 2 (2 wins vs player 5's 1 win).
    TestEqual(TEXT("Leader is player 2"), S.GetLeader(), 2);

    // Tie-break: lower index wins. Give player 5 another win.
    S.RecordWin(5);
    S.RecordWin(5);
    // Now player 5 has 3 wins, player 2 has 2.
    TestEqual(TEXT("Leader is now player 5"), S.GetLeader(), 5);

    // Make player 1 equal to player 5 (both 3 wins). Lower index (1) should win.
    S.RecordWin(1);
    S.RecordWin(1);
    S.RecordWin(1);
    // Player 1: 3, Player 2: 2, Player 5: 3.
    TestEqual(TEXT("Tie-break: lower index player 1 wins over player 5"), S.GetLeader(), 1);

    // Out-of-range RecordWin is silently ignored.
    S.RecordWin(-1);
    S.RecordWin(16);
    TestEqual(TEXT("Leader unchanged after out-of-range RecordWin"), S.GetLeader(), 1);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartySessionStateAdvance,
    "PartyButtons.Session.State.AdvanceGameCompletesAtLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartySessionStateAdvance::RunTest(const FString& Parameters)
{
    FPartySessionState S;
    S.GamesPerSession = 10;

    TestFalse(TEXT("Not complete at start"), S.IsSessionComplete());

    for (int32 i = 0; i < 9; i++)
    {
        S.AdvanceGame();
        TestFalse(FString::Printf(TEXT("Not complete after %d games"), i + 1),
                  S.IsSessionComplete());
    }

    S.AdvanceGame(); // 10th
    TestTrue(TEXT("Complete after 10 games"), S.IsSessionComplete());
    TestEqual(TEXT("GamesPlayed == 10"), S.GamesPlayed, 10);

    // Past the limit.
    S.AdvanceGame();
    TestTrue(TEXT("Still complete after 11 games"), S.IsSessionComplete());

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartySessionStateReset,
    "PartyButtons.Session.State.ResetClearsAll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartySessionStateReset::RunTest(const FString& Parameters)
{
    FPartySessionState S;
    S.GamesPerSession = 7; // non-default

    // Do some activity.
    S.RegisterPlayer(0);
    S.RegisterPlayer(4);
    S.RecordWin(0);
    S.RecordWin(0);
    S.RecordWin(4);
    S.AdvanceGame();
    S.AdvanceGame();
    S.AdvanceGame();
    S.CurrentGameIndex  = 3;
    S.CurrentPhase      = EPartyPhase::Minigame;
    S.IncrementAI();
    S.IncrementAI();

    S.ResetSession();

    // Live session state cleared.
    TestEqual(TEXT("Reset: GamesPlayed == 0"), S.GamesPlayed, 0);
    TestEqual(TEXT("Reset: CurrentGameIndex == INDEX_NONE"), S.CurrentGameIndex, static_cast<int32>(INDEX_NONE));
    TestEqual(TEXT("Reset: CurrentPhase == MainMenu"), S.CurrentPhase, EPartyPhase::MainMenu);
    TestEqual(TEXT("Reset: 0 registered"), S.GetRegisteredCount(), 0);
    TestFalse(TEXT("Reset: player 0 not registered"), S.IsPlayerRegistered(0));
    TestFalse(TEXT("Reset: player 4 not registered"), S.IsPlayerRegistered(4));
    TestEqual(TEXT("Reset: GetLeader == INDEX_NONE"), S.GetLeader(), static_cast<int32>(INDEX_NONE));
    TestEqual(TEXT("Reset: NumAIPlayers == 0"), S.NumAIPlayers, 0);

    // Rules and roster preserved.
    TestEqual(TEXT("GamesPerSession preserved"), S.GamesPerSession, 7);
    TestEqual(TEXT("Roster preserved"), S.GameRoster.Num(), 16);

    return true;
}

// --------------------------------------------------------------------------
// PartyButtons.Session.State.AI*
//
// Tests for the dev-only AI player slot logic (FPartySessionState::ComputeAISlots
// and friends). Pure code — no world, no actor, no engine subsystem.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyAIComputeSlotsReverseFill,
    "PartyButtons.Session.State.AIComputeSlotsReverseFill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyAIComputeSlotsReverseFill::RunTest(const FString& Parameters)
{
    TArray<bool> NoHumans;
    NoHumans.Init(false, FPartySessionState::NUM_PLAYERS);

    // NumAI=3, no humans -> highest three indices (15, 14, 13) are AI.
    TArray<bool> Slots;
    FPartySessionState::ComputeAISlots(NoHumans, 3, Slots);
    TestEqual(TEXT("OutIsAI sized to NUM_PLAYERS"), Slots.Num(), FPartySessionState::NUM_PLAYERS);
    for (int32 i = 0; i < FPartySessionState::NUM_PLAYERS; i++)
    {
        const bool bExpectedAI = (i == 15 || i == 14 || i == 13);
        TestEqual(FString::Printf(TEXT("Slot %d AI == %d"), i, bExpectedAI), Slots[i], bExpectedAI);
    }

    // NumAI=0 -> nothing is AI.
    FPartySessionState::ComputeAISlots(NoHumans, 0, Slots);
    for (bool b : Slots) { TestFalse(TEXT("NumAI=0: no slot is AI"), b); }

    // NumAI=16 -> every slot is AI.
    FPartySessionState::ComputeAISlots(NoHumans, 16, Slots);
    for (bool b : Slots) { TestTrue(TEXT("NumAI=16: every slot is AI"), b); }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyAIComputeSlotsHumanIntentWins,
    "PartyButtons.Session.State.AIComputeSlotsHumanIntentWins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyAIComputeSlotsHumanIntentWins::RunTest(const FString& Parameters)
{
    TArray<bool> Claimed;
    Claimed.Init(false, FPartySessionState::NUM_PLAYERS);
    Claimed[15] = true; // a human has claimed slot 15

    TArray<bool> Slots;
    FPartySessionState::ComputeAISlots(Claimed, 1, Slots);
    TestFalse(TEXT("Human-claimed slot 15 is never AI"), Slots[15]);
    TestTrue(TEXT("AI shifts to the next free slot (14)"), Slots[14]);

    // A second human claims 14 too — AI shifts again, to 13.
    Claimed[14] = true;
    FPartySessionState::ComputeAISlots(Claimed, 1, Slots);
    TestFalse(TEXT("Human-claimed slot 14 is never AI"), Slots[14]);
    TestTrue(TEXT("AI shifts to the next free slot (13)"), Slots[13]);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyAIComputeSlotsCapsAtAvailable,
    "PartyButtons.Session.State.AIComputeSlotsCapsAtAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyAIComputeSlotsCapsAtAvailable::RunTest(const FString& Parameters)
{
    TArray<bool> Claimed;
    Claimed.Init(false, FPartySessionState::NUM_PLAYERS);
    for (int32 i = 0; i < 10; i++) { Claimed[i] = true; } // 10 humans claim slots 0..9

    TArray<bool> Slots;
    FPartySessionState::ComputeAISlots(Claimed, 16, Slots); // ask for far more AI than fits

    int32 AICount = 0;
    for (bool b : Slots) { if (b) { ++AICount; } }

    // Only 6 slots (10..15) are free — AI is capped there, no overflow/crash.
    TestEqual(TEXT("AI capped to the 6 available slots"), AICount, 6);
    for (int32 i = 10; i < FPartySessionState::NUM_PLAYERS; i++)
    {
        TestTrue(FString::Printf(TEXT("Free slot %d is AI"), i), Slots[i]);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyAIIncrementDecrementClamp,
    "PartyButtons.Session.State.AIIncrementDecrementClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyAIIncrementDecrementClamp::RunTest(const FString& Parameters)
{
    FPartySessionState S;
    TestEqual(TEXT("Starts at 0"), S.NumAIPlayers, 0);

    // Decrementing below 0 stays at 0.
    S.DecrementAI();
    TestEqual(TEXT("Decrement below 0 clamps to 0"), S.NumAIPlayers, 0);

    // Incrementing past 16 stays at 16.
    for (int32 i = 0; i < 20; i++) { S.IncrementAI(); }
    TestEqual(TEXT("Increment past 16 clamps to 16"), S.NumAIPlayers, 16);

    S.DecrementAI();
    TestEqual(TEXT("Decrement from 16 -> 15"), S.NumAIPlayers, 15);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyAIIsSlotAIUsesRegisteredPlayers,
    "PartyButtons.Session.State.AIIsSlotAIUsesRegisteredPlayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyAIIsSlotAIUsesRegisteredPlayers::RunTest(const FString& Parameters)
{
    FPartySessionState S;
    S.RegisterPlayer(15);
    S.RegisterPlayer(0);
    S.IncrementAI();
    S.IncrementAI();

    TArray<bool> Expected;
    FPartySessionState::ComputeAISlots(S.RegisteredPlayers, S.NumAIPlayers, Expected);

    for (int32 i = 0; i < FPartySessionState::NUM_PLAYERS; i++)
    {
        TestEqual(FString::Printf(TEXT("IsSlotAI(%d) matches ComputeAISlots"), i), S.IsSlotAI(i), Expected[i]);
    }

    // Out-of-range is safely false.
    TestFalse(TEXT("IsSlotAI(-1) == false"), S.IsSlotAI(-1));
    TestFalse(TEXT("IsSlotAI(16) == false"), S.IsSlotAI(16));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyAIGetActiveParticipantCount,
    "PartyButtons.Session.State.AIGetActiveParticipantCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyAIGetActiveParticipantCount::RunTest(const FString& Parameters)
{
    FPartySessionState S;
    TestEqual(TEXT("0 humans, 0 AI -> 0 active"), S.GetActiveParticipantCount(), 0);

    S.RegisterPlayer(0);
    S.RegisterPlayer(1);
    TestEqual(TEXT("2 humans, 0 AI -> 2 active"), S.GetActiveParticipantCount(), 2);

    S.IncrementAI();
    S.IncrementAI();
    S.IncrementAI();
    TestEqual(TEXT("2 humans, 3 AI (disjoint) -> 5 active"), S.GetActiveParticipantCount(), 5);

    // Register every slot as human — AI has nowhere free, count stays at 16 (no double-count).
    for (int32 i = 0; i < FPartySessionState::NUM_PLAYERS; i++) { S.RegisterPlayer(i); }
    TestEqual(TEXT("16 humans, 3 AI (fully overlapped) -> still 16 active"), S.GetActiveParticipantCount(), 16);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyRosterHasSixteen,
    "PartyButtons.Session.Roster.HasSixteenUniqueGames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyRosterHasSixteen::RunTest(const FString& Parameters)
{
    FPartySessionState S; // ctor calls InitDefaultRoster()

    TestEqual(TEXT("Roster has 16 entries"), S.GameRoster.Num(), 16);

    TSet<int32>   Ids;
    TSet<FString> MapNames;

    for (int32 i = 0; i < S.GameRoster.Num(); i++)
    {
        const FPartyGameInfo& Info = S.GameRoster[i];

        // Id matches index.
        TestEqual(FString::Printf(TEXT("Id[%d] == %d"), i, i), Info.Id, i);

        // DisplayName is non-empty.
        TestFalse(FString::Printf(TEXT("DisplayName[%d] non-empty"), i),
                  Info.DisplayName.IsEmpty());

        // MapName has L_ prefix.
        TestTrue(FString::Printf(TEXT("MapName[%d] has L_ prefix"), i),
                 Info.MapName.ToString().StartsWith(TEXT("L_")));

        // Uniqueness.
        TestFalse(FString::Printf(TEXT("MapName[%d] is unique"), i),
                  MapNames.Contains(Info.MapName.ToString()));
        TestFalse(FString::Printf(TEXT("Id[%d] is unique"), i),
                  Ids.Contains(Info.Id));

        Ids.Add(Info.Id);
        MapNames.Add(Info.MapName.ToString());

        // MapName follows the L_GameX pattern (X = 'A' + i).
        const FString Expected = FString::Printf(TEXT("L_Game%c"), TEXT('A') + i);
        TestEqual(FString::Printf(TEXT("MapName[%d] == %s"), i, *Expected),
                  Info.MapName.ToString(), Expected);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyRosterFindByMapName,
    "PartyButtons.Session.Roster.FindGameByMapName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyRosterFindByMapName::RunTest(const FString& Parameters)
{
    FPartySessionState S;

    // Round-trip each roster entry.
    for (int32 i = 0; i < S.GameRoster.Num(); i++)
    {
        const FName MapName = S.GameRoster[i].MapName;
        const int32 Found   = S.FindGameByMapName(MapName);
        TestEqual(FString::Printf(TEXT("FindGameByMapName(%s) == %d"), *MapName.ToString(), i),
                  Found, i);
    }

    // Unknown names return INDEX_NONE.
    TestEqual(TEXT("FindGameByMapName(NAME_None) == INDEX_NONE"),
              S.FindGameByMapName(NAME_None), static_cast<int32>(INDEX_NONE));
    TestEqual(TEXT("FindGameByMapName(L_Unknown) == INDEX_NONE"),
              S.FindGameByMapName(FName(TEXT("L_Unknown"))), static_cast<int32>(INDEX_NONE));
    TestEqual(TEXT("FindGameByMapName(UEDPIE_L_GameA) == INDEX_NONE — callers must strip PIE prefix"),
              S.FindGameByMapName(FName(TEXT("UEDPIE_0_L_GameA"))), static_cast<int32>(INDEX_NONE));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartySessionStateSelectNextGame,
    "PartyButtons.Session.State.SelectNextGameDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartySessionStateSelectNextGame::RunTest(const FString& Parameters)
{
    FPartySessionState S;

    // Two streams with the same seed must produce the same sequence.
    FRandomStream RngA(12345);
    FRandomStream RngB(12345);

    constexpr int32 Iterations = 50;
    bool bAllMatch   = true;
    bool bAllInRange = true;

    for (int32 i = 0; i < Iterations; i++)
    {
        const int32 A = S.SelectNextGame(RngA);
        const int32 B = S.SelectNextGame(RngB);
        if (A != B) { bAllMatch   = false; }
        if (A < 0 || A >= 16) { bAllInRange = false; }
    }

    TestTrue(TEXT("Same seed -> same sequence"), bAllMatch);
    TestTrue(TEXT("All results in [0,15]"), bAllInRange);

    // Avoid-repeat: with 16 choices, we should rarely get the same result twice in a row.
    // Just verify the function doesn't hard-lock onto one value.
    FRandomStream RngC(99);
    S.CurrentGameIndex = 7; // set a "previous" game
    TSet<int32> Seen;
    for (int32 i = 0; i < 30; i++) { Seen.Add(S.SelectNextGame(RngC)); }
    TestTrue(TEXT("30 calls produce more than 1 unique result"), Seen.Num() > 1);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyRosterGameModeOverride,
    "PartyButtons.Session.Roster.GameModeOverridesUseReflectedClassNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyRosterGameModeOverride::RunTest(const FString& Parameters)
{
    FPartySessionState S;

    // Slots with a bespoke GameMode instead of the shared APartyMinigameGameMode:
    //   0 (L_GameA / "Reflex Rumble")  -> APartyArenaGameMode
    //   2 (L_GameC / "Octo Odyssey")   -> AOctoGameMode
    // See APartyGameModeBase::TravelToGame. Written as a set (not two
    // hardcoded slot checks) so a future slot 3, 4, ... needs no edit here —
    // only the "every non-empty override is well-formed" checks below do.
    const TSet<int32> ExpectedOverrideSlots = { 0, 2 };

    for (int32 i = 0; i < S.GameRoster.Num(); i++)
    {
        const FPartyGameInfo& Slot = S.GameRoster[i];
        const bool bHasOverride    = !Slot.GameModeClassPath.IsEmpty();

        TestEqual(FString::Printf(TEXT("Slot %d has a GameModeClassPath override iff expected"), i),
            bHasOverride, ExpectedOverrideSlots.Contains(i));

        if (!bHasOverride) { continue; }

        TestTrue(FString::Printf(TEXT("Slot %d override begins /Script/PartyButtons."), i),
            Slot.GameModeClassPath.StartsWith(TEXT("/Script/PartyButtons.")));

        // Pitfall guard: reflected class name must not contain the 'A' prefix.
        const int32 DotIndex = Slot.GameModeClassPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (DotIndex != INDEX_NONE)
        {
            const FString ClassName = Slot.GameModeClassPath.Mid(DotIndex + 1);
            TestFalse(FString::Printf(TEXT("Slot %d class name must not start with 'A'"), i),
                ClassName.StartsWith(TEXT("A")));
        }
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
