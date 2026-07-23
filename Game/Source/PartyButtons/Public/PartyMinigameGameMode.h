#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartyMinigameGameMode.generated.h"

/**
 * APartyMinigameGameMode
 *
 * Shared GameMode used by ALL 16 minigame maps (L_GameA..L_GameP).
 * Resolves which game it is by comparing the current map name against the roster.
 *
 * Rules:
 *   - The FIRST registered player to press their button wins.
 *   - Win is recorded, game count advances.
 *   - If GamesPerSession reached → Results. Otherwise → back to LevelSelect.
 *   - A guard flag (bWinnerDeclared) prevents double-fire on the same frame.
 *
 * PIE pitfall:
 *   GetWorld()->GetMapName() returns "UEDPIE_0_L_GameA" in PIE.
 *   We strip the PIE prefix via UWorld::RemovePIEPrefix before the roster lookup.
 *
 * HUD: displays the game's zany DisplayName in the center of the screen.
 */
UCLASS()
class PARTYBUTTONS_API APartyMinigameGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    virtual FString GetHudTitle() const override;

protected:
    virtual void BeginPlay() override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;

    /**
     * Record PlayerIndex's win, advance the session, and travel to Results
     * (session complete) or back to LevelSelect. Shared by every minigame's
     * win path — subclasses with different win conditions (e.g. last player
     * standing) call this once they've determined the winner, instead of
     * duplicating the record/advance/travel sequence.
     */
    virtual void DeclareWinner(int32 PlayerIndex);

    /**
     * Advance the session with no win recorded (e.g. a mutual-death draw),
     * then travel exactly like DeclareWinner. Guarded the same way.
     */
    virtual void DeclareNoContest();

    /** Roster index of the current game, resolved in BeginPlay. */
    int32 CurrentRosterIndex = INDEX_NONE;

    /** Human-readable name cached in BeginPlay for the HUD. */
    FString CurrentGameName;

    /** True once a winner (or no-contest) has been declared; prevents double-fire. */
    bool bWinnerDeclared = false;

private:
    /** Shared tail of DeclareWinner/DeclareNoContest: advance + travel. */
    void AdvanceAndTravel();
};
