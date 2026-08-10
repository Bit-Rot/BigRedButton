#pragma once

#include "CoreMinimal.h"
#include "PartyInputController.h"
#include "OctoPlayerController.generated.h"

/**
 * AOctoPlayerController
 *
 * APartyInputController with one value changed: the main button must be held for
 * a full second to count as a hold rather than the shared 0.6s.
 *
 * That second is a spec'd feel decision, not a tuning whim — the main button
 * hold is what COMMITS an entered name, and a commit that fires while the player
 * is still deciding is the one input mistake in this flow that cannot be undone.
 * A tap and a hold have to be unmistakably different gestures there.
 *
 * Scoped to a subclass rather than raised on APartyInputController because the
 * threshold is shared with the party session's "hold to go back", where 0.6s is
 * right and a full second reads as unresponsive. OctoOdyssey is a self-contained
 * level with its own GameMode, so a controller of its own costs nothing.
 */
UCLASS()
class PARTYBUTTONS_API AOctoPlayerController : public APartyInputController
{
    GENERATED_BODY()

public:
    AOctoPlayerController();
};
