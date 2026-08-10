#pragma once

#include "CoreMinimal.h"
#include "PartyFlowHUD.h"
#include "OctoOdyssey/OctoScores.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoHUD.generated.h"

class AOctoGameMode;

/**
 * AOctoHUD
 *
 * OctoOdyssey's own canvas HUD: main menu, running clock, score entry and the
 * side-by-side scoreboard.
 *
 * Derives from APartyFlowHUD rather than AHUD for two things it genuinely wants
 * to share — DrawCenteredText, and the generic dev tuning overlay (Tab) — and
 * overrides DrawHUD wholesale for everything else. It never consults
 * EPartyPhase: OctoOdyssey has no session and no phase, and the state it draws
 * from is AOctoGameMode::GetFlowState. If the auth GameMode turns out not to be
 * an AOctoGameMode (the level opened without its GameMode override — see
 * PartyFlowRouter.h's ?game= pitfall) it falls straight through to the base's
 * phase drawing, which is the visible symptom you want in that case rather than
 * a blank screen.
 *
 * Layout is computed from the canvas size and a fixed cell width rather than
 * from character counts, because the engine's small font is proportional and
 * space-padded columns in a proportional font do not line up.
 */
UCLASS()
class PARTYBUTTONS_API AOctoHUD : public APartyFlowHUD
{
    GENERATED_BODY()

protected:
    virtual void DrawHUD() override;

private:
    void DrawMainMenu(AOctoGameMode* GM);
    void DrawPlaying(AOctoGameMode* GM);
    void DrawScoreView(AOctoGameMode* GM);
    void DrawScoreEntry(AOctoGameMode* GM);

    /**
     * One top-ten table with its heading.
     *
     * EditLetterIndex is the rank whose name is being typed, or INDEX_NONE for a
     * read-only table; that row is drawn highlighted with its eight letters in
     * separate cells and a 1-8 player key underneath, so it is obvious which
     * button owns which letter.
     *
     * ExtraEntry, when set, is drawn UNNUMBERED below the last slot — the
     * "you didn't make it" consolation row. It is display-only and is never part
     * of Table (see OctoScores::NumSlots).
     */
    void DrawScoreTable(
        const TArray<FOctoScoreEntry>& Table,
        const FString&                 Heading,
        float                          LeftX,
        float                          TopY,
        int32                          EditRank,
        const FOctoScoreEntry*         ExtraEntry);

    /** Centre Text horizontally inside [CellX, CellX+CellW). */
    void DrawTextInCell(const FString& Text, float CellX, float CellW, float Y, FLinearColor Color, float Scale);

    /** Measured width of Text at Scale, for the cell centring above. */
    float MeasureText(const FString& Text, float Scale) const;

    /** Dark full-screen wash so canvas text stays readable over the 3D island. */
    void DrawBackdrop(float Alpha);

    // ---- Layout ------------------------------------------------------------

    static constexpr float TABLE_WIDTH   = 430.f;
    static constexpr float TABLE_GAP     = 40.f;   // between the two tables in ScoreView
    static constexpr float ROW_HEIGHT    = 30.f;
    static constexpr float RANK_WIDTH    = 46.f;
    static constexpr float LETTER_WIDTH  = 26.f;   // one name letter cell
    static constexpr float TIME_WIDTH    = 120.f;
    static constexpr float HEADING_SPACE = 46.f;   // heading row to first score row
};
