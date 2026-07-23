#include "PartyFlowHUD.h"
#include "PartyButtons.h"
#include "PartyGameModeBase.h"
#include "PartySessionSubsystem.h"
#include "PartySessionState.h"
#include "PartyTypes.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// ---- Private helpers -------------------------------------------------------

UPartySessionSubsystem* APartyFlowHUD::GetSession() const
{
    return UPartySessionSubsystem::Get(this);
}

APartyGameModeBase* APartyFlowHUD::GetFlowGM() const
{
    return GetWorld() ? GetWorld()->GetAuthGameMode<APartyGameModeBase>() : nullptr;
}

// ---- DrawHUD ---------------------------------------------------------------

void APartyFlowHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas) { return; }

    const UPartySessionSubsystem* S  = GetSession();
    APartyGameModeBase*           GM = GetFlowGM();

    // Guard: during map travel either or both may be null.
    if (!S) { return; }

    const FPartySessionState& State = S->GetState();
    const EPartyPhase Phase         = State.CurrentPhase;

    // One-time log per phase change so we can confirm what the HUD is seeing.
    if (Phase != LastLoggedPhase)
    {
        UE_LOG(LogPartyButtons, Log, TEXT("APartyFlowHUD: rendering phase %d"), static_cast<int32>(Phase));
        LastLoggedPhase = Phase;
    }

    switch (Phase)
    {
    case EPartyPhase::Main:
        // Blank; the map redirects immediately.
        break;

    case EPartyPhase::MainMenu:
        DrawMainMenu(GM);
        break;

    case EPartyPhase::Settings:
        DrawSettings(GM);
        break;

    case EPartyPhase::Lobby:
        DrawLobby(GM);
        break;

    case EPartyPhase::LevelSelect:
        DrawLevelSelect(GM, State);
        break;

    case EPartyPhase::Minigame:
        DrawMinigame(GM);
        break;

    case EPartyPhase::Results:
        DrawResults(State);
        break;

    default:
        break;
    }
}

// ---- Per-phase draw functions ----------------------------------------------

void APartyFlowHUD::DrawMainMenu(APartyGameModeBase* GM)
{
    // Dark background so the menu is visible on an empty map.
    DrawRect(FLinearColor(0.02f, 0.02f, 0.08f, 0.85f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

    // Options: 0 = Play, 1 = Settings
    static const TCHAR* Options[] = { TEXT("Play"), TEXT("Settings") };
    constexpr int32 NumOptions = 2;

    const int32 Selected = GM ? GM->GetSelectionIndex() : 0;

    DrawCenteredText(TEXT("PARTY BUTTONS"), 60.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 2.0f);
    DrawCenteredText(TEXT("Player buttons: cycle   Main button (Enter): confirm"), 120.f,
                     FLinearColor(0.6f, 0.6f, 0.6f, 1.f));

    const float OptionStartY = 200.f;
    const float OptionStep   = 60.f;

    for (int32 i = 0; i < NumOptions; i++)
    {
        const bool bSelected    = (i == Selected);
        const FLinearColor Color = bSelected
            ? FLinearColor(0.2f, 1.0f, 0.3f, 1.f)
            : FLinearColor(0.8f, 0.8f, 0.8f, 1.f);
        const FString Label = bSelected
            ? FString::Printf(TEXT(">> %s <<"), Options[i])
            : FString(Options[i]);
        DrawCenteredText(Label, OptionStartY + i * OptionStep, Color, 1.5f);
    }
}

void APartyFlowHUD::DrawSettings(APartyGameModeBase* GM)
{
    DrawCenteredText(TEXT("SETTINGS"), 60.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 2.0f);
    DrawCenteredText(TEXT("Player buttons: cycle   Tap: change value   Hold: back"), 120.f,
                     FLinearColor(0.6f, 0.6f, 0.6f, 1.f));

    if (!GM) { return; }

    // Use the base accessor if it's a settings GameMode (checked via interface).
    // For now just display what GetSelectionIndex + GetHudTitle show.
    const FString Body = FString::Printf(TEXT("Selected: option %d"), GM->GetSelectionIndex());
    DrawCenteredText(Body, 220.f, FLinearColor::White);
    DrawCenteredText(TEXT("(Full settings UI to be expanded)"), 300.f, FLinearColor(0.5f, 0.5f, 0.5f, 1.f));
    DrawCenteredText(TEXT("[Hold Enter to go back]"), 380.f, FLinearColor(0.5f, 0.8f, 1.f, 1.f));
}

void APartyFlowHUD::DrawLobby(APartyGameModeBase* GM)
{
    DrawRect(FLinearColor(0.02f, 0.05f, 0.02f, 0.85f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    DrawCenteredText(TEXT("LOBBY"), 30.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 2.0f);
    DrawCenteredText(TEXT("HOLD your button to join   Release = deactivate   Need 2+"), 80.f,
                     FLinearColor(0.8f, 0.8f, 0.8f, 1.f));

    // Countdown or waiting prompt
    const float Remaining = GM ? GM->GetCountdownRemaining() : -1.0f;
    if (Remaining >= 0.0f)
    {
        const FString Timer = FString::Printf(TEXT("Starting in: %.1fs — keep holding!"), Remaining);
        DrawCenteredText(Timer, 130.f, FLinearColor(0.3f, 1.f, 0.3f, 1.f), 1.5f);
    }
    else
    {
        DrawCenteredText(TEXT("Waiting for 2+ players to hold..."), 130.f,
                         FLinearColor(0.5f, 0.5f, 0.5f, 1.f));
    }

    // 4×4 grid — lit = joined
    DrawButtonGrid(
        [&](int32 i) { return GM ? GM->IsTileJoined(i) : false; },
        [&](int32 i) { return FString::Printf(TEXT("%d"), i + 1); },
        INDEX_NONE);
}

void APartyFlowHUD::DrawLevelSelect(APartyGameModeBase* GM, const FPartySessionState& State)
{
    DrawRect(FLinearColor(0.05f, 0.02f, 0.02f, 0.85f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    DrawCenteredText(TEXT("CHOOSE A LEVEL"), 30.f, FLinearColor(1.f, 0.5f, 0.1f, 1.f), 2.0f);
    DrawCenteredText(TEXT("Any button: next level   Enter: confirm   Hold Enter: back"), 80.f,
                     FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

    const int32 Highlight = GM ? GM->GetHighlightTile() : INDEX_NONE;

    // 4×4 grid — highlight is the moving cursor; label is the game DisplayName (abbreviated)
    DrawButtonGrid(
        [&](int32 i) { return false; }, // no cells are "joined" lit in LevelSelect
        [&](int32 i) -> FString
        {
            if (State.GameRoster.IsValidIndex(i))
            {
                // Truncate long names to fit in the cell (first word only)
                const FString& Name = State.GameRoster[i].DisplayName;
                FString Word;
                Name.Split(TEXT(" "), &Word, nullptr);
                return Word.IsEmpty() ? Name.Left(8) : Word.Left(8);
            }
            return TEXT("?");
        },
        Highlight);
}

void APartyFlowHUD::DrawMinigame(APartyGameModeBase* GM)
{
    DrawRect(FLinearColor(0.05f, 0.0f, 0.05f, 0.85f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    const FString Title = GM ? GM->GetHudTitle() : TEXT("???");
    const FString Subtitle = GM ? GM->GetHudSubtitle() : TEXT("");
    DrawCenteredText(Title, 150.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 3.0f);
    DrawCenteredText(Subtitle, 280.f, FLinearColor(0.8f, 0.8f, 0.8f, 1.f), 1.5f);
}

void APartyFlowHUD::DrawResults(const FPartySessionState& State)
{
    DrawRect(FLinearColor(0.02f, 0.02f, 0.0f, 0.85f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    DrawCenteredText(TEXT("RESULTS"), 30.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 2.5f);
    DrawCenteredText(FString::Printf(TEXT("Games played: %d"), State.GamesPlayed),
                     100.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

    // Leaderboard: list all players with at least 1 win, sorted descending.
    TArray<TPair<int32, int32>> Scores; // {WinCount, PlayerIndex}
    for (int32 i = 0; i < State.WinCounts.Num(); i++)
    {
        if (State.WinCounts[i] > 0)
        {
            Scores.Add({ State.WinCounts[i], i });
        }
    }
    Scores.Sort([](const TPair<int32,int32>& A, const TPair<int32,int32>& B)
    {
        return A.Key > B.Key; // descending by win count
    });

    const float ScoreStartY = 160.f;
    const float ScoreStep   = 50.f;

    if (Scores.IsEmpty())
    {
        DrawCenteredText(TEXT("No wins recorded."), ScoreStartY, FLinearColor(0.5f, 0.5f, 0.5f, 1.f));
    }

    for (int32 Rank = 0; Rank < Scores.Num(); Rank++)
    {
        const int32 Wins   = Scores[Rank].Key;
        const int32 Player = Scores[Rank].Value;
        const FLinearColor Color = (Rank == 0)
            ? FLinearColor(1.f, 0.85f, 0.1f, 1.f)  // gold for #1
            : FLinearColor(0.8f, 0.8f, 0.8f, 1.f);
        const FString Line = FString::Printf(TEXT("#%d  Player %d  —  %d win%s"),
            Rank + 1, Player + 1, Wins, Wins == 1 ? TEXT("") : TEXT("s"));
        DrawCenteredText(Line, ScoreStartY + Rank * ScoreStep, Color);
    }

    const float BackY = ScoreStartY + FMath::Max(1, Scores.Num()) * ScoreStep + 40.f;
    DrawCenteredText(TEXT("[Tap or hold Enter to continue]"), BackY,
                     FLinearColor(0.5f, 0.8f, 1.f, 1.f));
}

// ---- DrawButtonGrid --------------------------------------------------------

void APartyFlowHUD::DrawButtonGrid(
    TFunctionRef<bool(int32)>    IsLit,
    TFunctionRef<FString(int32)> GetLabel,
    int32                        HighlightIndex)
{
    if (!Canvas) { return; }

    const float Step = CELL_SIZE + CELL_MARGIN;

    for (int32 i = 0; i < GRID_COLS * GRID_ROWS; i++)
    {
        const int32 Row = i / GRID_COLS;
        const int32 Col = i % GRID_COLS;
        const float X   = GRID_START_X + Col * Step;
        const float Y   = GRID_START_Y + Row * Step;

        const bool bLit         = IsLit(i);
        const bool bHighlighted = (i == HighlightIndex);

        FLinearColor BoxColor;
        if (bHighlighted)
        {
            BoxColor = FLinearColor(1.0f, 0.55f, 0.0f, 1.f);  // orange highlight
        }
        else if (bLit)
        {
            BoxColor = FLinearColor(0.1f, 0.9f, 0.2f, 1.f);   // green: joined/lit
        }
        else
        {
            BoxColor = FLinearColor(0.15f, 0.15f, 0.15f, 0.8f); // dark grey: idle
        }

        DrawRect(BoxColor, X, Y, CELL_SIZE, CELL_SIZE);

        // Border
        const FLinearColor Border = FLinearColor(0.5f, 0.5f, 0.5f, 1.f);
        DrawLine(X,             Y,             X + CELL_SIZE, Y,             Border);
        DrawLine(X + CELL_SIZE, Y,             X + CELL_SIZE, Y + CELL_SIZE, Border);
        DrawLine(X + CELL_SIZE, Y + CELL_SIZE, X,             Y + CELL_SIZE, Border);
        DrawLine(X,             Y + CELL_SIZE, X,             Y,             Border);

        // Cell label
        const FString Label = GetLabel(i);
        if (!Label.IsEmpty())
        {
            DrawText(Label, FLinearColor::White, X + 4.f, Y + 4.f);
        }
    }
}

// ---- DrawCenteredText ------------------------------------------------------

void APartyFlowHUD::DrawCenteredText(const FString& Text, float Y, FLinearColor Color, float Scale)
{
    if (!Canvas) { return; }

    // Use the engine small font when available; DrawText accepts nullptr and uses its own default.
    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

    float TextW = 0.f;
    if (Font)
    {
        float TextH = 0.f;
        Canvas->TextSize(Font, Text, TextW, TextH, Scale, Scale);
    }
    else
    {
        // Approximate: ~10px per character at scale 1.0; centering will be close enough.
        TextW = Text.Len() * 10.f * Scale;
    }

    const float X = FMath::Max(0.f, (Canvas->SizeX - TextW) * 0.5f);
    DrawText(Text, Color, X, Y, Font, Scale);
}
