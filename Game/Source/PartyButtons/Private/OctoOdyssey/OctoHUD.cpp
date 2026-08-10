#include "OctoOdyssey/OctoHUD.h"
#include "OctoOdyssey/OctoGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
    const FLinearColor GGold     (1.00f, 0.90f, 0.10f, 1.f);
    const FLinearColor GWhite    (0.90f, 0.90f, 0.90f, 1.f);
    const FLinearColor GDim      (0.45f, 0.45f, 0.45f, 1.f);
    const FLinearColor GSelected (0.20f, 1.00f, 0.30f, 1.f);
    const FLinearColor GHint     (0.55f, 0.75f, 1.00f, 1.f);
    const FLinearColor GEditText (0.30f, 1.00f, 0.45f, 1.f);
    const FLinearColor GEditBand (0.08f, 0.30f, 0.12f, 0.85f);

    /** Placeholder glyphs for a slot nobody has scored into yet. */
    const TCHAR* const GEmptyLetter = TEXT("-");
    const TCHAR* const GEmptyTime   = TEXT("--:--.--");
}

void AOctoHUD::DrawHUD()
{
    if (!Canvas) { return; }

    AOctoGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AOctoGameMode>() : nullptr;

    if (!GM)
    {
        // Not our GameMode — most likely the level was opened without its
        // DefaultGameMode override and fell back to GlobalDefaultGameMode (see
        // PartyFlowRouter.h). Fall through to the party-phase HUD, which makes
        // that visible instead of showing nothing at all.
        Super::DrawHUD();
        return;
    }

    // AHUD::DrawHUD, not Super::DrawHUD: we want the base HUD's debug/hitbox
    // pass, but NOT APartyFlowHUD's EPartyPhase switch, which would draw a party
    // session screen underneath ours. OctoOdyssey has no phase.
    AHUD::DrawHUD();

    switch (GM->GetFlowState())
    {
    case EOctoFlowState::MainMenu:   DrawMainMenu(GM);   break;
    case EOctoFlowState::Playing:    DrawPlaying(GM);    break;
    case EOctoFlowState::ScoreEntry: DrawScoreEntry(GM); break;
    case EOctoFlowState::ScoreView:  DrawScoreView(GM);  break;
    }

    // Last, and outside the switch: the dev menu overlays whatever is running
    // rather than replacing it. Inherited wholesale from APartyFlowHUD.
    if (GM->GetDevMenuOpen())
    {
        DrawDevMenu(GM);
    }
}

// ---- Screens ---------------------------------------------------------------

void AOctoHUD::DrawMainMenu(AOctoGameMode* GM)
{
    // Light wash, not a curtain: the menu island and the course behind it are the
    // backdrop this screen was built for, so darkening them to nothing would
    // throw away the whole reason the camera is pointed where it is.
    DrawBackdrop(0.35f);

    DrawCenteredText(TEXT("OCTO ODYSSEY"), 70.f, GGold, 2.6f);
    DrawCenteredText(TEXT("Any player button: cycle      Main button: select"), 140.f, GDim);

    const int32 Selected = GM->GetSelectionIndex();

    const float OptionStartY = 260.f;
    const float OptionStep   = 62.f;

    for (int32 i = 0; i < AOctoGameMode::NumMenuOptions; i++)
    {
        const bool bIsSelected = (i == Selected);
        const FString Label = bIsSelected
            ? FString::Printf(TEXT(">>  %s  <<"), AOctoGameMode::GetMenuOptionLabel(i))
            : FString(AOctoGameMode::GetMenuOptionLabel(i));

        DrawCenteredText(Label, OptionStartY + i * OptionStep, bIsSelected ? GSelected : GWhite, 1.6f);
    }
}

void AOctoHUD::DrawPlaying(AOctoGameMode* GM)
{
    // No backdrop at all — this is live gameplay and the canvas must not sit
    // between the players and the octopus.
    DrawCenteredText(OctoScores::FormatTime(GM->GetRunSeconds()), 24.f, GGold, 2.4f);

    const FString CourseLine = FString::Printf(TEXT("%s COURSE"), OctoCourse::ToString(GM->GetActiveCourse()));
    DrawCenteredText(CourseLine.ToUpper(), 86.f, GDim);

    DrawCenteredText(GM->GetHudSubtitle(), Canvas->SizeY - 46.f, GDim);
    DrawCenteredText(TEXT("[Hold the main button to give up]"), Canvas->SizeY - 26.f, GHint);
}

void AOctoHUD::DrawScoreView(AOctoGameMode* GM)
{
    DrawBackdrop(0.6f);

    DrawCenteredText(TEXT("TOP SCORES"), 50.f, GGold, 2.4f);

    const float TotalWidth = TABLE_WIDTH * 2.f + TABLE_GAP;
    const float LeftX      = (Canvas->SizeX - TotalWidth) * 0.5f;
    const float TopY       = 130.f;

    DrawScoreTable(GM->GetScoreTable(EOctoCourse::Normal), TEXT("NORMAL"),
        LeftX, TopY, INDEX_NONE, nullptr);

    DrawScoreTable(GM->GetScoreTable(EOctoCourse::Hard), TEXT("HARD"),
        LeftX + TABLE_WIDTH + TABLE_GAP, TopY, INDEX_NONE, nullptr);

    DrawCenteredText(TEXT("[Main button: back to the menu]"), Canvas->SizeY - 50.f, GHint);
}

void AOctoHUD::DrawScoreEntry(AOctoGameMode* GM)
{
    DrawBackdrop(0.6f);

    const int32 Rank    = GM->GetPendingRank();
    const bool  bPlaced = (Rank != INDEX_NONE);

    DrawCenteredText(bPlaced ? TEXT("CONGRATULATIONS!") : TEXT("BETTER LUCK NEXT TIME"),
        50.f, bPlaced ? GGold : GDim, 2.2f);

    DrawCenteredText(
        FString::Printf(TEXT("%s COURSE  —  %s"),
            *FString(OctoCourse::ToString(GM->GetActiveCourse())).ToUpper(),
            *OctoScores::FormatTime(GM->GetRunSeconds())),
        108.f, GWhite, 1.3f);

    // Build the table AS IT WILL LOOK once committed, by running the same insert
    // the subsystem will run. Anything else risks previewing a placement the
    // commit then disagrees with.
    const FOctoScoreEntry Pending(GM->GetPendingName(), GM->GetRunSeconds());

    TArray<FOctoScoreEntry> Display = GM->GetScoreTable(GM->GetActiveCourse());
    const int32 DisplayRank = OctoScores::Insert(Display, Pending);

    const float LeftX = (Canvas->SizeX - TABLE_WIDTH) * 0.5f;
    const float TopY  = 170.f;

    DrawScoreTable(Display, FString(OctoCourse::ToString(GM->GetActiveCourse())).ToUpper(),
        LeftX, TopY, DisplayRank, bPlaced ? nullptr : &Pending);

    if (bPlaced)
    {
        DrawCenteredText(TEXT("Buttons 1-8 cycle your eight letters   —   hold a button to run through them"),
            Canvas->SizeY - 72.f, GDim);
        DrawCenteredText(TEXT("[HOLD the main button for a second to enter your name]"),
            Canvas->SizeY - 48.f, GHint);
    }
    else
    {
        DrawCenteredText(TEXT("[HOLD the main button to return to the menu]"),
            Canvas->SizeY - 48.f, GHint);
    }
}

// ---- Table -----------------------------------------------------------------

void AOctoHUD::DrawScoreTable(
    const TArray<FOctoScoreEntry>& Table,
    const FString&                 Heading,
    float                          LeftX,
    float                          TopY,
    int32                          EditRank,
    const FOctoScoreEntry*         ExtraEntry)
{
    DrawTextInCell(Heading, LeftX, TABLE_WIDTH, TopY, GGold, 1.6f);

    const float NameX  = LeftX + RANK_WIDTH;
    const float NameW  = LETTER_WIDTH * OctoScores::NameLength;
    const float TimeX  = NameX + NameW + 12.f;

    // A running cursor, not TopY + i * ROW_HEIGHT: the row being edited grows an
    // extra half-row underneath it for the 1-8 player key, and everything below
    // has to move down with it.
    float Y = TopY + HEADING_SPACE;

    for (int32 Slot = 0; Slot < OctoScores::NumSlots; Slot++)
    {
        const bool bIsEditRow = (Slot == EditRank);
        const bool bHasEntry  = Table.IsValidIndex(Slot);

        if (bIsEditRow)
        {
            DrawRect(GEditBand, LeftX - 6.f, Y - 3.f, TABLE_WIDTH + 12.f, ROW_HEIGHT);
        }

        const FLinearColor RowColor = bIsEditRow ? GEditText : (bHasEntry ? GWhite : GDim);

        DrawTextInCell(FString::Printf(TEXT("%d."), Slot + 1), LeftX, RANK_WIDTH, Y, bHasEntry ? GGold : GDim, 1.2f);

        // Letter by letter into fixed-width cells. The engine's small font is
        // proportional, so a space-padded string would leave the columns of ten
        // different names visibly ragged — this is what keeps them square.
        for (int32 L = 0; L < OctoScores::NameLength; L++)
        {
            const FString Glyph = bHasEntry && Table[Slot].Name.IsValidIndex(L)
                ? FString::Chr(Table[Slot].Name[L])
                : FString(GEmptyLetter);

            DrawTextInCell(Glyph, NameX + L * LETTER_WIDTH, LETTER_WIDTH, Y, RowColor, 1.4f);
        }

        DrawTextInCell(bHasEntry ? OctoScores::FormatTime(Table[Slot].TimeSeconds) : FString(GEmptyTime),
            TimeX, TIME_WIDTH, Y, RowColor, 1.2f);

        Y += ROW_HEIGHT;

        if (bIsEditRow)
        {
            // Which button drives which letter, directly beneath the letter it
            // drives. Player i owns letter i with no claiming step, so the mapping
            // only has to be READ, never negotiated.
            for (int32 L = 0; L < OctoScores::NameLength; L++)
            {
                DrawTextInCell(FString::FromInt(L + 1), NameX + L * LETTER_WIDTH, LETTER_WIDTH, Y, GDim, 0.9f);
            }
            Y += ROW_HEIGHT * 0.75f;
        }
    }

    if (!ExtraEntry) { return; }

    // The slot-11 consolation row: UNNUMBERED, because it is not a slot. It is
    // never stored (see OctoScores::NumSlots) and is dropped the moment this
    // screen is left.
    Y += 8.f;
    DrawLine(LeftX, Y - 4.f, LeftX + TABLE_WIDTH, Y - 4.f, GDim);

    for (int32 L = 0; L < OctoScores::NameLength; L++)
    {
        const FString Glyph = ExtraEntry->Name.IsValidIndex(L) ? FString::Chr(ExtraEntry->Name[L]) : FString(TEXT(" "));
        DrawTextInCell(Glyph, NameX + L * LETTER_WIDTH, LETTER_WIDTH, Y, GDim, 1.4f);
    }

    DrawTextInCell(OctoScores::FormatTime(ExtraEntry->TimeSeconds), TimeX, TIME_WIDTH, Y, GDim, 1.2f);
}

// ---- Helpers ---------------------------------------------------------------

float AOctoHUD::MeasureText(const FString& Text, float Scale) const
{
    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
    if (!Canvas || !Font)
    {
        // Same approximation APartyFlowHUD::DrawCenteredText falls back to.
        return Text.Len() * 10.f * Scale;
    }

    float W = 0.f;
    float H = 0.f;
    Canvas->TextSize(Font, Text, W, H, Scale, Scale);
    return W;
}

void AOctoHUD::DrawTextInCell(const FString& Text, float CellX, float CellW, float Y, FLinearColor Color, float Scale)
{
    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
    const float W = MeasureText(Text, Scale);
    DrawText(Text, Color, CellX + (CellW - W) * 0.5f, Y, Font, Scale);
}

void AOctoHUD::DrawBackdrop(float Alpha)
{
    if (!Canvas) { return; }

    DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, Alpha), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
}
