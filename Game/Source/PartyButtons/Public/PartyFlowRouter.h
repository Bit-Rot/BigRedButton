#pragma once

#include "CoreMinimal.h"
#include "PartyTypes.h"

/**
 * PartyFlow namespace — central routing table for phase-to-map/GameMode lookups.
 *
 * All map travel in PartyButtons goes through GetRoute() so that:
 *   - Map names and GameMode class paths are defined in exactly one place.
 *   - Tests can assert the routing table is correct without touching the engine.
 *   - Adding a new phase only requires updating this file.
 *
 * GameModeClassPath convention (critical UE pitfall):
 *   OpenLevel's ?game= option uses the REFLECTED class name, which drops the 'A'
 *   prefix. So APartyLobbyGameMode's path is:
 *     "/Script/PartyButtons.PartyLobbyGameMode"   (NOT "APartyLobbyGameMode")
 *   Getting this wrong silently falls back to GlobalDefaultGameMode with no error.
 *
 * Minigame special case:
 *   GetRoute(EPartyPhase::Minigame) returns NAME_None for MapName because the
 *   map is determined per-selected-game (from FPartySessionState::GameRoster).
 *   Callers fill in the MapName from the roster entry before calling OpenLevel.
 */
namespace PartyFlow
{
    /**
     * Return the fixed phase route for the given phase.
     * For Minigame, MapName is NAME_None — fill it from the roster at call time.
     */
    PARTYBUTTONS_API FPartyPhaseRoute GetRoute(EPartyPhase Phase);

    /**
     * Build the ?game= option string for the given GameModeClassPath.
     * e.g. "/Script/PartyButtons.PartyLobbyGameMode" ->
     *      "?game=/Script/PartyButtons.PartyLobbyGameMode"
     */
    PARTYBUTTONS_API FString BuildGameModeOption(const FString& GameModeClassPath);
}
