#include "Gameplay/Combat/BaseMMOProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "MyGameInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/DataStructs.h"

ABaseMMOProjectile::ABaseMMOProjectile()
{
    PrimaryActorTick.bCanEverTick = false;

    // ---- Collision sphere ------------------------------------------------
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    CollisionSphere->InitSphereRadius(30.0f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    CollisionSphere->SetGenerateOverlapEvents(true);
    RootComponent = CollisionSphere;

    // ---- Projectile movement ---------------------------------------------
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionSphere;
    ProjectileMovement->InitialSpeed        = DefaultSpeed;
    ProjectileMovement->MaxSpeed            = DefaultSpeed;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce       = false;
    ProjectileMovement->ProjectileGravityScale = 0.0f; // spells don't arc by default; override in BP

    // ---- Trail Niagara ---------------------------------------------------
    TrailVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailVFX"));
    TrailVFX->SetupAttachment(RootComponent);
    TrailVFX->bAutoActivate = false; // activated after SetupProjectile assigns the asset

    // ---- Flight audio ----------------------------------------------------
    FlightAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FlightAudio"));
    FlightAudio->SetupAttachment(RootComponent);
    FlightAudio->bAutoActivate = false;
}

void ABaseMMOProjectile::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("[Projectile] BeginPlay: actor='%s' instigator='%s' collision='%s'"),
        *GetName(),
        GetInstigator() ? *GetInstigator()->GetName() : TEXT("NULL"),
        *CollisionSphere->GetCollisionProfileName().ToString());

    // Ignore the instigator (caster) so we don't immediately overlap with them
    if (GetInstigator())
    {
        CollisionSphere->IgnoreActorWhenMoving(GetInstigator(), true);
        GetInstigator()->MoveIgnoreActorAdd(this);
    }

    // Disable collision until SetupProjectile() is called. SpawnActor() triggers BeginPlay
    // synchronously, before the caller has a chance to call SetupProjectile(), so physics
    // can fire an immediate overlap if the spawn location is inside a combatable's collision.
    // That would deliver a hit with an uninitialized SkillSlug / CasterId = 0.
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Wire overlap delegate
    CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABaseMMOProjectile::OnSphereBeginOverlap);

    // Safety lifetime — removes the actor even if target was never hit
    if (UWorld* W = GetWorld())
    {
        FTimerHandle LifetimeHandle;
        W->GetTimerManager().SetTimer(LifetimeHandle, [WeakSelf = TWeakObjectPtr<ABaseMMOProjectile>(this)]()
        {
            if (WeakSelf.IsValid() && !WeakSelf->IsPendingKillPending())
            {
                UE_LOG(LogTemp, Warning, TEXT("[Projectile] LIFETIME EXPIRED — overlap never fired! actor='%s' bHasHit=%d"),
                    *WeakSelf->GetName(), (int)WeakSelf->bHasHit);
                WeakSelf->Destroy();
            }
        }, MaxLifetime, false);
    }
}

void ABaseMMOProjectile::SetupProjectile(const FString& InSkillSlug, int32 InCasterId, AActor* InTarget, float Speed)
{
    SkillSlug = InSkillSlug;
    CasterId  = InCasterId;

    UE_LOG(LogTemp, Warning, TEXT("[Projectile] SetupProjectile: skill='%s' casterId=%d target='%s' speed=%.0f"),
        *InSkillSlug, InCasterId,
        IsValid(InTarget) ? *InTarget->GetName() : TEXT("NULL — straight-line flight, overlap uses ICombatable check"),
        Speed);

    if (IsValid(InTarget))
    {
        TargetActor = InTarget;
        ProjectileMovement->bIsHomingProjectile   = true;
        ProjectileMovement->HomingAccelerationMagnitude = 6000.0f;
        // SetHomingTarget requires the scene component that the movement tracks
        if (USceneComponent* TargetRoot = InTarget->GetRootComponent())
        {
            ProjectileMovement->HomingTargetComponent = TargetRoot;
            UE_LOG(LogTemp, Warning, TEXT("[Projectile] Homing enabled → target root component: '%s'"),
                *TargetRoot->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Projectile] No homing target — projectile flies straight. Overlap will fire on ANY ICombatable."));
    }

    float UseSpeed = (Speed > 0.0f) ? Speed : DefaultSpeed;
    ProjectileMovement->InitialSpeed = UseSpeed;
    ProjectileMovement->MaxSpeed     = UseSpeed;

    // Look up skill definition for trail + flight sound
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return;

    USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository();
    if (!Repo) return;

    const FSkillDefinitionData& Def = Repo->GetDefinition(InSkillSlug);

    // Trail VFX — set on the NiagaraComponent so the BP can override it via EditDefault
    if (!Def.castEndEffectNiagara.IsNull())
    {
        if (UNiagaraSystem* TrailSystem = Def.castEndEffectNiagara.LoadSynchronous())
        {
            TrailVFX->SetAsset(TrailSystem);
            TrailVFX->Activate(true);
        }
    }

    // Flight sound — plays looping while in flight
    if (!Def.castEndSound.IsNull())
    {
        if (USoundBase* FlightSnd = Def.castEndSound.LoadSynchronous())
        {
            FlightAudio->SetSound(FlightSnd);
            FlightAudio->Play();
        }
    }

    // Enable collision now that SkillSlug and CasterId are set.
    // Doing this last prevents the frame-0 overlap that would fire
    // if the spawn location is inside a combatable's collision volume.
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABaseMMOProjectile::OnSphereBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    UE_LOG(LogTemp, Warning, TEXT("[Projectile] OnSphereBeginOverlap: other='%s' comp='%s' bHasHit=%d targetValid=%d"),
        IsValid(OtherActor) ? *OtherActor->GetName() : TEXT("NULL"),
        OtherComp         ? *OtherComp->GetName()   : TEXT("NULL"),
        (int)bHasHit,
        (int)TargetActor.IsValid());

    if (bHasHit) return;
    if (!IsValid(OtherActor)) return;

    // If we have a specific target, only react to that actor to prevent early detonation
    if (TargetActor.IsValid() && OtherActor != TargetActor.Get())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Projectile] Overlap SKIPPED — not our homing target (target='%s')"),
            *TargetActor.Get()->GetName());
        return;
    }

    // Accept any ICombatable if no specific target was set
    if (!TargetActor.IsValid())
    {
        const bool bIsCombatable = OtherActor->GetClass()->ImplementsInterface(UCombatable::StaticClass())
            || OtherActor->Implements<UCombatable>();
        UE_LOG(LogTemp, Warning, TEXT("[Projectile] No homing target — bIsCombatable=%d for '%s'"),
            (int)bIsCombatable, *OtherActor->GetName());
        if (!bIsCombatable) return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[Projectile] IMPACT accepted → calling OnProjectileImpact"));
    OnProjectileImpact(GetActorLocation());
}

void ABaseMMOProjectile::OnProjectileImpact(const FVector& ImpactLocation)
{
    UE_LOG(LogTemp, Warning, TEXT("[Projectile] OnProjectileImpact: skill='%s' loc=(%.0f,%.0f,%.0f)"),
        *SkillSlug, ImpactLocation.X, ImpactLocation.Y, ImpactLocation.Z);

    bHasHit = true;

    // Stop moving and disable collision immediately to prevent queued overlap re-entry.
    ProjectileMovement->StopMovementImmediately();
    SetActorEnableCollision(false);
    FlightAudio->Stop();

    // Hide the entire actor immediately.
    // Destroy() in UE is deferred to end-of-frame; without this the actor (including
    // TrailVFX) stays visible and renders at the impact point for that extra frame.
    SetActorHiddenInGame(true);

    // Kill the trail NiagaraComponent's renderer and simulation.
    // DeactivateImmediate() alone stops new emission but already-emitted CPU particles
    // continue until their own particle lifetime expires.
    // Setting visibility false on the component hides the renderer output immediately.
    if (TrailVFX)
    {
        TrailVFX->SetVisibility(false);
        TrailVFX->DeactivateImmediate();
    }

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (GI)
    {
        // Notify combat manager: flush deferred damage/heal result at visual impact moment.
        // Use NotifyProjectileImpact (not NotifyHitPoint) so that projectile-waiting results
        // are not blocked by the bSkipProjectileWaiters guard inside FlushPendingResults.
        if (UCombatSystemManager* CombatMgr = GI->GetCombatSystemManager())
        {
            CombatMgr->NotifyProjectileImpact(CasterId);
        }

        // Spawn impact VFX from skill definition
        if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

            if (!Def.hitEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* ImpactVFX = Def.hitEffectNiagara.LoadSynchronous())
                {
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, ImpactLocation, FRotator::ZeroRotator);
                }
            }

            if (!Def.hitSound.IsNull())
            {
                if (USoundBase* ImpactSnd = Def.hitSound.LoadSynchronous())
                {
                    UGameplayStatics::PlaySoundAtLocation(this, ImpactSnd, ImpactLocation);
                }
            }
        }
    }

    Destroy();
}
