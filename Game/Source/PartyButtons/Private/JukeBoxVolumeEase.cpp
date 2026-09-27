#include "JukeBoxVolumeEase.h"

void FJukeBoxVolumeEase::SetImmediate(float Volume)
{
    Current  = FMath::Max(0.f, Volume);
    Start    = Current;
    Target   = Current;
    Elapsed  = 0.f;
    Duration = 0.f;
}

void FJukeBoxVolumeEase::EaseTo(float InTarget, float Seconds)
{
    if (Seconds <= 0.f)
    {
        SetImmediate(InTarget);
        return;
    }

    // From Current, not from the old Start: an interrupted ease picks up where
    // it actually is, which is what keeps a quick back-and-forth from popping.
    Start    = Current;
    Target   = FMath::Max(0.f, InTarget);
    Elapsed  = 0.f;
    Duration = Seconds;
}

float FJukeBoxVolumeEase::Advance(float DeltaSeconds)
{
    if (!IsEasing()) { return Current; }

    Elapsed = FMath::Min(Elapsed + FMath::Max(0.f, DeltaSeconds), Duration);

    const float Alpha = FMath::SmoothStep(0.f, 1.f, Elapsed / Duration);
    Current = FMath::Lerp(Start, Target, Alpha);

    // Land exactly on the target rather than a float's width off it, so
    // GetCurrent() == GetTarget() is a reliable "done" check for callers.
    if (Elapsed >= Duration)
    {
        Current = Target;
    }

    return Current;
}
