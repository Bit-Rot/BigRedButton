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
    DrawCenteredText(TEXT("LOBBY"), 20.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 2.0f);
    DrawCenteredText(TEXT("HOLD your button to join   Release = deactivate   Need 2+"), 55.f,
                     FLinearColor(0.8f, 0.8f, 0.8f, 1.f));

    // Countdown or waiting prompt
    const float Remaining = GM ? GM->GetCountdownRemaining() : -1.0f;
    if (Remaining >= 0.0f)
    {
        const FString Timer = FString::Printf(TEXT("Starting in: %.1fs — keep holding!"), Remaining);
        DrawCenteredText(Timer, 90.f, FLinearColor(0.3f, 1.f, 0.3f, 1.f), 1.5f);
    }
    else
    {
        DrawCenteredText(TEXT("Waiting for 2+ players to hold..."), 90.f,
                         FLinearColor(0.5f, 0.5f, 0.5f, 1.f));
    }

    // Dev-only reminder: Up/Down adjusts the AI player count (see APartyLobbyGameMode::OnDevIncrement/Decrement).
    const UPartySessionSubsystem* S = GetSession();
    const int32 AICount = S ? S->GetNumAIPlayers() : 0;
    const FString DevLine = FString::Printf(TEXT("[DEV] Up/Down: add/remove AI players — currently %d"), AICount);
    DrawCenteredText(DevLine, 125.f, FLinearColor(0.4f, 0.6f, 0.9f, 1.f));

    // 4×4 grid — lit (green) = joined by a human, AI (blue) = AI-filled
    DrawButtonGrid(
        [&](int32 i) { return GM ? GM->IsTileJoined(i) : false; },
        [&](int32 i) { return GM ? GM->IsTileAI(i) : false; },
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
        [&](int32 i) { return false; }, // no AI concept in LevelSelect
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
    // Games that don't opt in to the intro overlay (default) keep the legacy
    // full-screen title + subtitle. Opt-in games (see APartyArenaGameMode) get
    // a dialog during tutorial, floating number labels during discovery, and
    // no HUD overlay at all once play begins.
    const EPartyOverlayPhase OverlayPhase = GM
        ? GM->GetOverlayPhase()
        : EPartyOverlayPhase::None;

    switch (OverlayPhase)
    {
    case EPartyOverlayPhase::Tutorial:
        DrawTutorialDialog(GM);
        return;

    case EPartyOverlayPhase::Discovery:
        DrawPawnMarkers(GM);
        return;

    case EPartyOverlayPhase::Playing:
        // Arena is live — draw nothing over it.
        return;

    case EPartyOverlayPhase::None:
    default:
        break;
    }

    DrawRect(FLinearColor(0.05f, 0.0f, 0.05f, 0.85f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
    const FString Title = GM ? GM->GetHudTitle() : TEXT("???");
    const FString Subtitle = GM ? GM->GetHudSubtitle() : TEXT("");
    DrawCenteredText(Title, 150.f, FLinearColor(1.f, 0.9f, 0.1f, 1.f), 3.0f);
    DrawCenteredText(Subtitle, 280.f, FLinearColor(0.8f, 0.8f, 0.8f, 1.f), 1.5f);
}

void APartyFlowHUD::DrawTutorialDialog(APartyGameModeBase* GM)
{
    if (!Canvas || !GM) { return; }

    // Subtle dim so the arena is visibly behind the dialog (spec: this is a
    // dialog, not a full-screen takeover).
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

    const float DialogW = FMath::Min(Canvas->SizeX * 0.7f, 820.f);
    const float DialogH = 260.f;
    const float DialogX = (Canvas->SizeX - DialogW) * 0.5f;
    const float DialogY = (Canvas->SizeY - DialogH) * 0.5f;

    DrawRect(FLinearColor(0.05f, 0.05f, 0.10f, 0.95f), DialogX, DialogY, DialogW, DialogH);

    const FLinearColor Border(0.85f, 0.75f, 0.15f, 1.f);
    DrawLine(DialogX,           DialogY,           DialogX + DialogW, DialogY,           Border);
    DrawLine(DialogX + DialogW, DialogY,           DialogX + DialogW, DialogY + DialogH, Border);
    DrawLine(DialogX + DialogW, DialogY + DialogH, DialogX,           DialogY + DialogH, Border);
    DrawLine(DialogX,           DialogY + DialogH, DialogX,           DialogY,           Border);

    const FString Title       = GM->GetHudTitle();
    const FString Instructions = GM->GetTutorialText();
    const float   Remaining   = GM->GetTutorialRemainingSeconds();

    DrawCenteredText(Title,        DialogY + 24.f,  FLinearColor(1.f, 0.9f, 0.1f, 1.f), 2.0f);
    DrawCenteredText(Instructions, DialogY + 105.f, FLinearColor(0.9f, 0.9f, 0.9f, 1.f), 1.3f);

    const FString Prompt = (Remaining >= 0.f)
        ? FString::Printf(TEXT("Hold your button to start   (auto-start in %.1fs)"), Remaining)
        : FString(TEXT("Hold your button to start"));
    DrawCenteredText(Prompt, DialogY + 195.f, FLinearColor(0.4f, 1.0f, 0.5f, 1.f), 1.3f);
}

void APartyFlowHUD::DrawPawnMarkers(APartyGameModeBase* GM)
{
    if (!Canvas || !GM) { return; }

    const TArray<FPartyPawnMarker> Markers = GM->GetPawnMarkers();
    UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;

    for (const FPartyPawnMarker& M : Markers)
    {
        // Project falls back to (0,0,-1) style outputs when the point is
        // behind the camera — Z <= 0 filters those out safely.
        const FVector Screen = Project(M.WorldLocation);
        if (Screen.Z <= 0.f) { continue; }

        const FString Label = FString::Printf(TEXT("%d"), M.PlayerIndex + 1);

        // Held → bright white flash, otherwise the player's assigned tint so
        // "which one is me?" is answerable at a glance during discovery.
        const FLinearColor Color = M.bHeld
            ? FLinearColor(1.f, 1.f, 1.f, 1.f)
            : M.Color;

        constexpr float LabelScale = 2.2f;
        float TextW = 0.f, TextH = 0.f;
        if (Font)
        {
            Canvas->TextSize(Font, Label, TextW, TextH, LabelScale, LabelScale);
        }
        else
        {
            TextW = 22.f * LabelScale;
            TextH = 22.f * LabelScale;
        }

        const float X = Screen.X - TextW * 0.5f;
        const float Y = Screen.Y - TextH - 40.f; // hover above the pawn's projected center

        DrawText(Label, Color, X, Y, Font, LabelScale);
    }
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
    TFunctionRef<bool(int32)>    IsAI,
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
        const bool bIsAI        = IsAI(i);
        const bool bHighlighted = (i == HighlightIndex);

        // Precedence: highlight > lit (human/joined) > AI > idle — never both.
        FLinearColor BoxColor;
        if (bHighlighted)
        {
            BoxColor = FLinearColor(1.0f, 0.55f, 0.0f, 1.f);  // orange highlight
        }
        else if (bLit)
        {
            BoxColor = FLinearColor(0.1f, 0.9f, 0.2f, 1.f);   // green: joined/lit (human)
        }
        else if (bIsAI)
        {
            BoxColor = FLinearColor(0.15f, 0.35f, 0.75f, 0.9f); // blue: AI-filled (dev-only)
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
