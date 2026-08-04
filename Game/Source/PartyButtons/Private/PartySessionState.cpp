#include "PartySessionState.h"

// ---- Construction ----------------------------------------------------------

FPartySessionState::FPartySessionState()
{
    RegisteredPlayers.SetNum(NUM_PLAYERS);
    WinCounts.SetNum(NUM_PLAYERS);
    InitDefaultRoster();
}

// ---- Initialization / reset ------------------------------------------------

void FPartySessionState::InitDefaultRoster()
{
    // 16 zany alliterative names, A–P (matching L_GameA..L_GameP).
    static const TCHAR* Names[NUM_PLAYERS] = {
        TEXT("Reflex Rumble"),
        TEXT("Bumper Bonanza"),
        TEXT("Cannon Carnage"),
        TEXT("Dizzy Derby"),
        TEXT("Electric Escape"),
        TEXT("Frantic Frenzy"),
        TEXT("Gravity Gauntlet"),
        TEXT("Hectic Hurdles"),
        TEXT("Icy Insanity"),
        TEXT("Jumbled Jamboree"),
        TEXT("Kooky Kickoff"),
        TEXT("Lava Lunacy"),
        TEXT("Mayhem Mountain"),
        TEXT("Nutty Nosedive"),
        TEXT("Outrageous Onslaught"),
        TEXT("Pandemonium Plaza"),
    };

    GameRoster.Reset(NUM_PLAYERS);
    for (int32 i = 0; i < NUM_PLAYERS; i++)
    {
        FPartyGameInfo Info;
        Info.Id          = i;
        Info.DisplayName = Names[i];
        Info.MapName     = FName(*FString::Printf(TEXT("L_Game%c"), TEXT('A') + i));
        GameRoster.Add(Info);
    }

    // Slot 0 (L_GameA / "Reflex Rumble") is the first real minigame — a spin,
    // charge, and reflect duel — and uses its own GameMode instead of the
    // shared "first press wins" APartyMinigameGameMode. Reflected class name,
    // no 'A' prefix (see AI/design/architecture.md travel-wiring pitfall).
    GameRoster[0].GameModeClassPath = TEXT("/Script/PartyButtons.PartyArenaGameMode");

    // Slot 2 (L_GameC / "Octo Odyssey") is the second real minigame — a
    // QWOP-style co-op physics game — and likewise overrides the shared
    // GameMode. Reflected class name, no 'A' prefix (same pitfall as above).
    GameRoster[2].DisplayName        = TEXT("Octo Odyssey");
    GameRoster[2].GameModeClassPath  = TEXT("/Script/PartyButtons.OctoGameMode");
}

void FPartySessionState::ResetSession()
{
    for (bool& b : RegisteredPlayers) { b = false; }
    for (int32& w : WinCounts)        { w = 0; }
    GamesPlayed      = 0;
    CurrentGameIndex = INDEX_NONE;
    CurrentPhase     = EPartyPhase::MainMenu;
    NumAIPlayers     = 0;
}

// ---- Player registration ---------------------------------------------------

void FPartySessionState::RegisterPlayer(int32 PlayerIndex)
{
    if (IsValidPlayerIndex(PlayerIndex))
    {
        RegisteredPlayers[PlayerIndex] = true;
    }
}

bool FPartySessionState::IsPlayerRegistered(int32 PlayerIndex) const
{
    return IsValidPlayerIndex(PlayerIndex) && RegisteredPlayers[PlayerIndex];
}

int32 FPartySessionState::GetRegisteredCount() const
{
    int32 Count = 0;
    for (bool b : RegisteredPlayers)
    {
        if (b) { ++Count; }
    }
    return Count;
}

// ---- Game tracking ---------------------------------------------------------

void FPartySessionState::RecordWin(int32 PlayerIndex)
{
    if (IsValidPlayerIndex(PlayerIndex))
    {
        ++WinCounts[PlayerIndex];
    }
}

void FPartySessionState::AdvanceGame()
{
    ++GamesPlayed;
}

bool FPartySessionState::IsSessionComplete() const
{
    return GamesPlayed >= GamesPerSession;
}

// ---- AI players --------------------------------------------------------------

/*static*/ void FPartySessionState::ComputeAISlots(const TArray<bool>& ClaimedByHuman, int32 NumAI, TArray<bool>& OutIsAI)
{
    OutIsAI.Init(false, NUM_PLAYERS);

    int32 Remaining = FMath::Clamp(NumAI, 0, NUM_PLAYERS);
    for (int32 i = NUM_PLAYERS - 1; i >= 0 && Remaining > 0; i--)
    {
        const bool bClaimed = ClaimedByHuman.IsValidIndex(i) && ClaimedByHuman[i];
        if (bClaimed) { continue; }

        OutIsAI[i] = true;
        --Remaining;
    }
}

bool FPartySessionState::IsSlotAI(int32 PlayerIndex) const
{
    if (!IsValidPlayerIndex(PlayerIndex)) { return false; }

    TArray<bool> AISlots;
    ComputeAISlots(RegisteredPlayers, NumAIPlayers, AISlots);
    return AISlots[PlayerIndex];
}

int32 FPartySessionState::GetActiveParticipantCount() const
{
    TArray<bool> AISlots;
    ComputeAISlots(RegisteredPlayers, NumAIPlayers, AISlots);

    int32 Count = 0;
    for (int32 i = 0; i < NUM_PLAYERS; i++)
    {
        if (IsPlayerRegistered(i) || AISlots[i]) { ++Count; }
    }
    return Count;
}

void FPartySessionState::IncrementAI()
{
    NumAIPlayers = FMath::Clamp(NumAIPlayers + 1, 0, NUM_PLAYERS);
}

void FPartySessionState::DecrementAI()
{
    NumAIPlayers = FMath::Clamp(NumAIPlayers - 1, 0, NUM_PLAYERS);
}

// ---- Roster access ---------------------------------------------------------

void FPartySessionState::SetCurrentGame(int32 RosterIndex)
{
    CurrentGameIndex = RosterIndex;
}

int32 FPartySessionState::SelectNextGame(FRandomStream& Rng) const
{
    if (GameRoster.Num() == 0)
    {
        return INDEX_NONE;
    }

    // Avoid the immediately-previous game when possible.
    const int32 Avoid = CurrentGameIndex;
    if (GameRoster.Num() == 1)
    {
        return 0;
    }

    int32 Candidate = Rng.RandHelper(GameRoster.Num());
    if (Candidate == Avoid)
    {
        // Bump forward one slot (wrapping) to avoid the repeat.
        Candidate = (Candidate + 1) % GameRoster.Num();
    }
    return Candidate;
}

int32 FPartySessionState::FindGameByMapName(FName MapName) const
{
    // Accept just the short name (e.g. "L_GameA") — callers must strip PIE prefixes
    // and package paths before calling this (e.g. UWorld::RemovePIEPrefix).
    for (int32 i = 0; i < GameRoster.Num(); i++)
    {
        if (GameRoster[i].MapName == MapName)
        {
            return i;
        }
    }
    return INDEX_NONE;
}

// ---- Results ---------------------------------------------------------------

int32 FPartySessionState::GetLeader() const
{
    int32 BestIndex = INDEX_NONE;
    int32 BestCount = 0;

    for (int32 i = 0; i < WinCounts.Num(); i++)
    {
        // Strictly greater than: ties go to the lowest index (deterministic).
        if (WinCounts[i] > BestCount)
        {
            BestCount = WinCounts[i];
            BestIndex = i;
        }
    }
    return BestIndex;
}
