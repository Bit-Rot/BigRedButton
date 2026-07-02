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

private:
    /** Roster index of the current game, resolved in BeginPlay. */
    int32 CurrentRosterIndex = INDEX_NONE;

    /** Human-readable name cached in BeginPlay for the HUD. */
    FString CurrentGameName;

    /** True once a winner has been declared; prevents double-fire. */
    bool bWinnerDeclared = false;
};
