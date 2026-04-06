#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseMMOProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class UAudioComponent;

/**
 * Base class for all skill projectiles (fireballs, arrows, bolts, etc.).
 *
 * Usage:
 *   1. Create a Blueprint child class, set up mesh/trail/sound assets there.
 *   2. Assign that BP class to FSkillDefinitionData::projectileClass.
 *   3. In PlayCombatSoundEvent(CastRelease) the projectile is spawned and
 *      SetupProjectile() is called automatically — no extra setup required.
 *
 * On impact:
 *   - Spawns hitEffectNiagara from the skill definition at the hit location.
 *   - Plays hitSound from the skill definition.
 *   - Calls CombatSystemManager::NotifyHitPoint(CasterId) so the deferred
 *     combat result is flushed at the correct visual moment.
 *   - Destroys itself.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API ABaseMMOProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABaseMMOProjectile();

    // ------------------------------------------------------------------ //
    // Components                                                           //
    // ------------------------------------------------------------------ //

    /** Root collision sphere. Overlap-only, not blocking. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
    USphereComponent* CollisionSphere;

    /** Drives the projectile forward. Homing is enabled when a target exists. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
    UProjectileMovementComponent* ProjectileMovement;

    /** Trail / body VFX. Asset is set at runtime via SetupProjectile(). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
    UNiagaraComponent* TrailVFX;

    /** Looping flight sound. Assigned at runtime via SetupProjectile(). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
    UAudioComponent* FlightAudio;

    // ------------------------------------------------------------------ //
    // Configuration                                                        //
    // ------------------------------------------------------------------ //

    /** Slug of the skill that created this projectile (DataTable lookup key). */
    UPROPERTY(BlueprintReadWrite, Category = "Projectile")
    FString SkillSlug;

    /** Actor ID of the caster. Used to flush pending combat results on impact. */
    UPROPERTY(BlueprintReadWrite, Category = "Projectile")
    int32 CasterId = 0;

    /**
     * Auto-destroy after this many seconds even if the target was never reached.
     * Keeps the scene clean when the target dies before the projectile arrives.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float MaxLifetime = 8.0f;

    /** Default travel speed used when SetupProjectile() is called without a speed argument. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "100.0"))
    float DefaultSpeed = 1200.0f;

    // ------------------------------------------------------------------ //
    // API                                                                  //
    // ------------------------------------------------------------------ //

    /**
     * Configure the projectile immediately after spawning.
     * @param InSkillSlug  DataTable row key — resolves hitSound, hitEffectNiagara, trail, flight sound.
     * @param InCasterId   Id of the caster (for NotifyHitPoint).
     * @param InTarget     Optional homing target. Pass nullptr for straight-line travel.
     * @param Speed        Launch speed in Unreal units/s. 0 = use DefaultSpeed.
     */
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void SetupProjectile(const FString& InSkillSlug, int32 InCasterId, AActor* InTarget, float Speed = 0.0f);

protected:
    virtual void BeginPlay() override;

private:
    TWeakObjectPtr<AActor> TargetActor;

    UFUNCTION()
    void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    void OnProjectileImpact(const FVector& ImpactLocation);
};
