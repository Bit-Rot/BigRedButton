#pragma once

#include "CoreMinimal.h"

/**
 * FJukeBoxVolumeEase
 *
 * One track's volume, and the ease that is moving it. Pure value type — no
 * world, no audio component — so AJukeBox's timing is unit-testable without an
 * audio device (see JukeBoxTest.cpp).
 *
 * The ease is smoothstep (ease in AND out), not linear: a linear fade on a
 * volume multiplier has an audible "corner" at both ends, which is exactly the
 * seam this exists to hide.
 *
 * Retargeting mid-ease starts the new ease from wherever the old one had got
 * to, so flipping between two targets quickly (menu -> play -> menu) never
 * jumps.
 */
struct PARTYBUTTONS_API FJukeBoxVolumeEase
{
    FJukeBoxVolumeEase() = default;
    explicit FJukeBoxVolumeEase(float InVolume) { SetImmediate(InVolume); }

    /** Jump straight to Volume, cancelling any ease in flight. */
    void SetImmediate(float Volume);

    /** Ease from the current volume to Target over Seconds. Seconds <= 0 is SetImmediate. */
    void EaseTo(float Target, float Seconds);

    /** Advance the ease by DeltaSeconds. Returns the new current volume. */
    float Advance(float DeltaSeconds);

    float GetCurrent() const { return Current; }
    float GetTarget()  const { return Target; }
    bool  IsEasing()   const { return Duration > 0.f && Elapsed < Duration; }

private:
    float Current  = 1.f;
    float Start    = 1.f;
    float Target   = 1.f;
    float Elapsed  = 0.f;
    float Duration = 0.f;
};
