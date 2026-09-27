#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JukeBoxVolumeEase.h"
#include "JukeBox.generated.h"

class UAudioComponent;
class USoundBase;

/** One named track on an AJukeBox, as authored in the Details panel. */
USTRUCT(BlueprintType)
struct PARTYBUTTONS_API FJukeBoxTrack
{
    GENERATED_BODY()

    /** The key every AJukeBox call uses. Must be unique on its jukebox. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JukeBox")
    FName Name;

    /**
     * What to play. Looping is a property of the asset (SoundWave -> Looping),
     * not of the jukebox — a music bed that should loop is imported that way.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JukeBox")
    TObjectPtr<USoundBase> Sound;

    /** Starting volume multiplier. EaseTrackVolume / SetTrackVolume move it from here at runtime. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JukeBox", meta = (ClampMin = "0.0", UIMax = "2.0"))
    float Volume = 1.f;

    /** Start playing at BeginPlay. Off means it waits for PlayTrack. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "JukeBox")
    bool bAutoPlay = true;
};

/**
 * AJukeBox
 *
 * A placeable music player: any number of tracks, each keyed by a name, each
 * with its own volume, each able to ease to a new volume over time. Drop one in
 * a level, fill in Tracks, and drive it by name from game code.
 *
 * Game-agnostic on purpose — it lives at the module root, not under
 * OctoOdyssey/, and knows nothing about menus or runs. Deciding WHEN the music
 * changes is the game mode's job (see AOctoGameMode::ApplyMusicForFlowState);
 * this only knows HOW.
 *
 * Each track gets its own non-spatialised UAudioComponent, created on first use
 * rather than in BeginPlay. That is deliberate: the order in which actors
 * receive BeginPlay is not defined, so a game mode that sets a volume from its
 * own BeginPlay may well get here first. Everything it asks for is remembered
 * and applied when the track actually starts.
 *
 * Unknown track names log a warning and do nothing — a typo in a name is a
 * content bug worth seeing, but never worth a crash.
 */
UCLASS()
class PARTYBUTTONS_API AJukeBox : public AActor
{
    GENERATED_BODY()

public:
    AJukeBox();

    /**
     * Add a track, or replace the one already named TrackName. Works in the
     * editor (undoable — it is how AI/add_octo_jukebox.py fills the level's
     * jukebox, via call_method, which needs no Python struct wrapper) and at
     * runtime (a bAutoPlay track added after BeginPlay starts immediately).
     */
    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    void AddTrack(FName TrackName, USoundBase* Sound, float Volume = 1.f, bool bAutoPlay = true);

    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    void PlayTrack(FName TrackName);

    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    void StopTrack(FName TrackName);

    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    bool IsTrackPlaying(FName TrackName) const;

    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    bool HasTrack(FName TrackName) const;

    /** Set a track's volume right now, cancelling any ease in flight. */
    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    void SetTrackVolume(FName TrackName, float Volume);

    /**
     * Ease a track's volume from wherever it is now to TargetVolume over
     * Seconds (smoothstep). Calling again mid-ease retargets from the current
     * value, so rapid back-and-forth never jumps. Seconds <= 0 is SetTrackVolume.
     */
    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    void EaseTrackVolume(FName TrackName, float TargetVolume, float Seconds);

    /** The volume the track is at this frame (mid-ease included). 0 for an unknown name. */
    UFUNCTION(BlueprintCallable, Category = "JukeBox")
    float GetTrackVolume(FName TrackName) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "JukeBox", meta = (TitleProperty = "Name"))
    TArray<FJukeBoxTrack> Tracks;

private:
    /** Size the runtime arrays to Tracks, seeding each volume from its authored value. Safe before BeginPlay. */
    void EnsureRuntime();

    /** Index into Tracks for TrackName, or INDEX_NONE (with a warning) if unknown. */
    int32 FindTrack(FName TrackName) const;

    /** The track's audio component, created and registered on first call. */
    UAudioComponent* GetOrCreateComponent(int32 Index);

    /** Push the current eased volume into the track's audio component, if it has one yet. */
    void ApplyVolume(int32 Index);

    /** Tick only while something is easing — a jukebox at rest costs nothing. */
    void RefreshTickEnabled();

    /** Index-parallel with Tracks once EnsureRuntime has run. */
    TArray<FJukeBoxVolumeEase> Volumes;

    /** Index-parallel with Tracks; null until that track is first played. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UAudioComponent>> TrackComponents;
};
