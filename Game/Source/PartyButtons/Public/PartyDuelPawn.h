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
 * of killing the pawn; otherwise contact is lethal (Die()).
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

    /** Forwarded from APartyArenaGameMode::OnPlayerButton. */
    void NotifyPressed();

    /** Forwarded from APartyArenaGameMode::OnPlayerButtonReleased. */
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

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float TapWindowSeconds = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float ReflectActiveSeconds = 0.1f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float MinBulletSpeed = 400.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    float MaxBulletSpeed = 1600.f;

    UPROPERTY(EditDefaultsOnly, Category = "PartyDuel")
    TSubclassOf<APartyBullet> BulletClass;

private:
    enum class EDuelState : uint8 { Idle, Charging, ReflectWindow };

    void EnterIdle();
    void FireBullet(float HeldSeconds);
    void Die();

    UPROPERTY(VisibleAnywhere, Category = "PartyDuel")
    TObjectPtr<USphereComponent> SphereComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyDuel")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, Category = "PartyDuel")
    TObjectPtr<UArrowComponent> FacingArrow;

    int32 PlayerIndex = INDEX_NONE;
    EDuelState State  = EDuelState::Idle;

    double PressWorldTime       = 0.0;
    double ReflectWindowEndTime = 0.0;

    bool bAlive = true;
};
