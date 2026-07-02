#include "PartyFlowRouter.h"

namespace PartyFlow
{

FPartyPhaseRoute GetRoute(EPartyPhase Phase)
{
    // IMPORTANT: GameModeClassPath must use the reflected name (no 'A' prefix).
    // e.g. APartyLobbyGameMode -> "/Script/PartyButtons.PartyLobbyGameMode"
    // Wrong: "/Script/PartyButtons.APartyLobbyGameMode" (silently falls back to GlobalDefaultGameMode).

    switch (Phase)
    {
    case EPartyPhase::Main:
        return { FName(TEXT("L_Main")),        TEXT("/Script/PartyButtons.PartyMainGameMode") };

    case EPartyPhase::MainMenu:
        return { FName(TEXT("L_MainMenu")),    TEXT("/Script/PartyButtons.PartyMainMenuGameMode") };

    case EPartyPhase::Settings:
        return { FName(TEXT("L_Settings")),    TEXT("/Script/PartyButtons.PartySettingsGameMode") };

    case EPartyPhase::Lobby:
        return { FName(TEXT("L_Lobby")),       TEXT("/Script/PartyButtons.PartyLobbyGameMode") };

    case EPartyPhase::LevelSelect:
        return { FName(TEXT("L_LevelSelect")), TEXT("/Script/PartyButtons.PartyLevelSelectGameMode") };

    case EPartyPhase::Minigame:
        // MapName is intentionally NAME_None — the caller must fill it from the roster.
        return { NAME_None,                    TEXT("/Script/PartyButtons.PartyMinigameGameMode") };

    case EPartyPhase::Results:
        return { FName(TEXT("L_Results")),     TEXT("/Script/PartyButtons.PartyResultsGameMode") };

    default:
        // Fallback: should not be reached. Route to MainMenu to be safe.
        return { FName(TEXT("L_MainMenu")),    TEXT("/Script/PartyButtons.PartyMainMenuGameMode") };
    }
}

FString BuildGameModeOption(const FString& GameModeClassPath)
{
    return FString::Printf(TEXT("?game=%s"), *GameModeClassPath);
}

} // namespace PartyFlow
