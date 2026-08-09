#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OctoOdyssey/OctoTuning.h"
#include "OctoTuningSubsystem.generated.h"

/**
 * UOctoTuningSubsystem
 *
 * Holds the one live FOctoTuning the dev tuning menu edits.
 *
 * It is a GameInstanceSubsystem for exactly the reason UPartySessionSubsystem
 * is one: the GameInstance and its subsystems are the ONLY objects that survive
 * UGameplayStatics::OpenLevel. The menu's Accept button reloads the course
 * (AOctoGameMode::ReloadCourse), so the edited values have to live somewhere
 * that outlives the world — otherwise every Accept would throw away the very
 * change it was applying.
 *
 * Persisted to Config/OctoTuning.ini, loaded on Initialize and written whenever
 * the dev menu closes, so a tuning session survives closing the editor. The
 * file is written key-per-field from the descriptor table, which means it stays
 * hand-editable and picks up new tunables with no extra work:
 *
 *     [OctoTuning]
 *     ExtendSpeed=900.000000
 *     ArmGripFraction=0.350000
 *     bPushAtImpactPoint=True
 *
 * The JSON line the menu logs on Accept is still the record you paste back over
 * FOctoTuning's member initialisers to promote a tuning to a shipping default.
 *
 * The hazard of any on-disk override is that it silently masks a C++ default you
 * later change — a value you edited months ago keeps winning and the new default
 * never takes effect. Two things keep that honest: LoadFromIni logs every value
 * it overrode, and the dev menu carries a RESET TO DEFAULTS action that clears
 * the file (see AOctoGameMode::ActivateDevMenuRow).
 */
UCLASS()
class PARTYBUTTONS_API UOctoTuningSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Convenience getter. Returns nullptr outside a game world — an editor
     * viewport has no GameInstance, and callers are expected to fall back to
     * their own FOctoTuning defaults rather than treat that as an error.
     */
    static UOctoTuningSubsystem* Get(const UObject* WorldContext);

    const FOctoTuning& GetTuning() const { return Tuning; }

    void SetTuning(const FOctoTuning& InTuning) { Tuning = InTuning; }

    /** Back to the shipping defaults (FOctoTuning's member initialisers), and clear the ini. */
    void ResetTuning();

    /** Write every field to Config/OctoTuning.ini. Called when the dev menu closes. */
    void SaveToIni() const;

    /**
     * Overlay Config/OctoTuning.ini onto the defaults. Fields absent from the
     * file keep their C++ default, so adding a tunable never needs a migration
     * and deleting a line is a per-field reset.
     */
    void LoadFromIni();

    /** Absolute path of the tuning ini. Public so tests and logs can name it. */
    static FString GetIniPath();

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    FOctoTuning Tuning;
};
