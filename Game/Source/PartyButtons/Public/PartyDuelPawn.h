#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PartyDuelPawn.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UArrowComponent;
class APartyBullet;
struct FHitResult;

/** Broadcast once, right before a duel pawn destroys itself in Die(). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyDuelPawnDied, int32 /*PlayerIndex*/);

/**
 * APartyDuelPawn
 *
 * The player actor for the Reflex Rumble minigame (APartyArenaGameMode).
 * NOT possessed — per the project's player model (see AI/design/architecture.md
 * and PartyInputController.h), there is one PlayerController fanning out by
 * PlayerIndex; APartyArenaGameMode calls NotifyPressed()/NotifyReleased() on
 * the matching pawn directly instead of routing input through possession.
 *
 * State machine:
 *   Idle          -> spins on Z at IdleSpinRateDegPerSec.
 *   Charging      -> (button held) spin stops; tracks hold duration.
 *   On release, PartyDuel::ClassifyRelease decides what happens:
 *     Reflect (held <= TapWindowSeconds)      -> opens a brief reflect window, spin resumes.
 *     Cancel  (held < MinChargeSeconds)       -> no shot, spin resumes.
 *     Shoot   (held >= MinChargeSeconds)      -> fires a APartyBullet along facing at a
 *                                                speed from PartyDuel::ChargeToSpeed, spin resumes.
 *   ReflectWindow -> spins as in Idle; expires back to Idle after ReflectActiveSeconds.
 *
 * A bullet that overlaps this pawn while the reflect window is open is
 * redirected off the sphere's surface normal (see NotifyHitByBullet) instead
 * of dealing damage. Otherwise a non-reflected hit increments HitsTaken;
 * Die() fires once HitsTaken reaches MaxHits (default 3). The body dims as
 * damage accumulates so remaining HP reads at a glance.
 */
UCLASS()
class PARTYBUTTONS_API APartyDuelPawn : public APawn
{
    GENERATED_BODY()

public:
    APartyDuelPawn();

    FOnPartyDuelPawnDied OnDied;

    /** Assign this pawn's identity and tint. Call once, immediately after spawning. */
    void Init(int32 InPlayerIndex, const FLinearColor& InColor);

    int32 GetPlayerIndex() const { return PlayerIndex; }

    // ---- Read-only accessors for APartyArenaGameMode::TickAI (see PartyDuelAI.h) ----
    // AI is allowed to READ these — a human sees exactly the same info on the
    // top-down camera (spin rate, roughly how long a charge takes, own size) —
    // it just may never WRITE pawn state directly (see class comment).

    float GetIdleSpinRateDegPerSec() const { return IdleSpinRateDegPerSec; }
    float GetMinChargeSeconds() const { return MinChargeSeconds; }
    float GetMaxChargeSeconds() const { return MaxChargeSeconds; }

    /** Collision sphere radius in Unreal units (cm), e.g. for AI ricochet/impact math. */
    float GetSphereRadiusUnits() const { return SphereRadiusMeters * 100.f; }

    /** Forwarded from APartyArenaGameMode::OnGameplayButton. */
    void NotifyPressed();

    /** Forwarded from APartyArenaGameMode::OnGameplayButtonReleased. */
    void NotifyReleased();

    /**
     * Called by APartyBullet when it overlaps this pawn.
     * Returns true if the bullet was reflected (redirected, still alive) —
     * the caller should leave it alone. Returns false if it killed this pawn
     * — the caller (the bullet) should destroy itself.
     */
    bool NotifyHitByBullet(APartyBullet* Bullet, const FHitResult& Hit);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // ---- Tunables — EditDefaultsOnly so these are easy to iterate on ----

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float SphereRadiusMeters = 0.25f; // 0.5m diameter sphere

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float IdleSpinRateDegPerSec = 90.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float MinChargeSeconds = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float MaxChargeSeconds = 2.0f;

    // A comfortable human "quick tap" (press + release) runs ~100–180ms end to
    // end — the original 0.05s window sat below that floor, so almost every
    // tap fell through into the Cancel band and never entered ReflectWindow.
    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float TapWindowSeconds = 0.20f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float ReflectActiveSeconds = 0.25f;

    /**
     * Body color applied while the reflect window is open. Bright white by
     * default so it reads clearly against every per-player tint. Values > 1
     * push into HDR — visible even if the base material clamps to LDR.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    FLinearColor ReflectFlashColor = FLinearColor(2.0f, 2.0f, 2.0f, 1.f);

    /**
     * How many non-reflected bullet hits kill this pawn. 1 = one-shot kill
     * (original behavior); the default of 3 gives players two forgiveness
     * hits before elimination. Clamped to >= 1 at runtime.
     */
    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel", meta = (ClampMin = "1"))
    int32 MaxHits = 3;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float MinBulletSpeed = 400.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float MaxBulletSpeed = 1000.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    TSubclassOf<APartyBullet> BulletClass;

private:
    enum class EDuelState : uint8 { Idle, Charging, ReflectWindow };

    void EnterIdle();
    void FireBullet(float HeldSeconds);
    void Die();

    /** Push the current body color to the MID — base tint or reflect flash. */
    void ApplyBodyColor(bool bReflectFlash);

    UPROPERTY(VisibleAnywhere, Category = "PartyDuel")
    TObjectPtr<USphereComponent> SphereComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyDuel")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyDuel")
    TObjectPtr<UArrowComponent> FacingArrow;

    /** Dynamic material on MeshComponent — cached so we can re-tint at runtime. */
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> BodyMID;

    /** Per-player tint set in Init, restored when leaving ReflectWindow. */
    FLinearColor BaseColor = FLinearColor::White;

    int32 PlayerIndex = INDEX_NONE;
    EDuelState State  = EDuelState::Idle;

    double PressWorldTime       = 0.0;
    double ReflectWindowEndTime = 0.0;

    /** Non-reflected bullet hits taken so far. Die() fires at MaxHits. */
    int32 HitsTaken = 0;

    bool bAlive = true;
};
