#pragma once

#include "CoreMinimal.h"
#include "PartyGameModeBase.h"
#include "PartyLevelSelectGameMode.generated.h"

/**
 * APartyLevelSelectGameMode
 *
 * Used by L_LevelSelect. Any registered player's button press advances the
 * highlight cursor one tile (wrapping). The main button confirms the current
 * tile and travels to that minigame. Main-hold returns to lobby.
 *
 * HUD: 4×4 roster grid with the current tile highlighted orange.
 */
UCLASS()
class PARTYBUTTONS_API APartyLevelSelectGameMode : public APartyGameModeBase
{
    GENERATED_BODY()

public:
    APartyLevelSelectGameMode() = default;

    virtual FString GetHudTitle()      const override { return TEXT("Choose a Level"); }
    virtual int32   GetHighlightTile() const override { return HighlightIndex; }

protected:
    virtual void BeginPlay() override;
    virtual void OnPlayerButton(int32 PlayerIndex) override;
    virtual void OnMainTap()  override;
    virtual void OnMainHold() override;

private:
    int32 HighlightIndex = 0;
    bool  bConfirmed     = false;   // guard against double-travel

    static constexpr int32 NUM_TILES = 16;
};
