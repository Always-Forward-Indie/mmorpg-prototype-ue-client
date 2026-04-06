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

    // Ignore the instigator (caster) so we don't immediately overlap with them
    if (GetInstigator())
    {
        CollisionSphere->IgnoreActorWhenMoving(GetInstigator(), true);
        GetInstigator()->MoveIgnoreActorAdd(this);
    }

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
                WeakSelf->Destroy();
            }
        }, MaxLifetime, false);
    }
}

void ABaseMMOProjectile::SetupProjectile(const FString& InSkillSlug, int32 InCasterId, AActor* InTarget, float Speed)
{
    SkillSlug = InSkillSlug;
    CasterId  = InCasterId;

    if (IsValid(InTarget))
    {
        TargetActor = InTarget;
        ProjectileMovement->bIsHomingProjectile   = true;
        ProjectileMovement->HomingAccelerationMagnitude = 6000.0f;
        // SetHomingTarget requires the scene component that the movement tracks
        if (USceneComponent* TargetRoot = InTarget->GetRootComponent())
        {
            ProjectileMovement->HomingTargetComponent = TargetRoot;
        }
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
}

void ABaseMMOProjectile::OnSphereBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    if (!IsValid(OtherActor)) return;

    // If we have a specific target, only react to that actor to prevent early detonation
    if (TargetActor.IsValid() && OtherActor != TargetActor.Get()) return;

    // Accept any ICombatable if no specific target was set
    if (!TargetActor.IsValid())
    {
        const bool bIsCombatable = OtherActor->GetClass()->ImplementsInterface(UCombatable::StaticClass())
            || OtherActor->Implements<UCombatable>();
        if (!bIsCombatable) return;
    }

    OnProjectileImpact(GetActorLocation());
}

void ABaseMMOProjectile::OnProjectileImpact(const FVector& ImpactLocation)
{
    // Stop moving immediately
    ProjectileMovement->StopMovementImmediately();
    FlightAudio->Stop();

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (GI)
    {
        // Notify combat manager: flush deferred damage/heal result at visual impact moment
        if (UCombatSystemManager* CombatMgr = GI->GetCombatSystemManager())
        {
            CombatMgr->NotifyHitPoint(CasterId);
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
