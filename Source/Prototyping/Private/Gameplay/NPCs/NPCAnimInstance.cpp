// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/NPCs/NPCAnimInstance.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

UNPCAnimInstance::UNPCAnimInstance()
{
}

void UNPCAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwnerNPC = Cast<ABasicNPC>(TryGetPawnOwner());

    // Bind the end delegate once so we don't re-bind every tick
    ActionMontageEndedDelegate.BindUObject(this, &UNPCAnimInstance::OnActionMontageEnded);
}

void UNPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwnerNPC.IsValid()) return;

    // Drive Speed / bIsMoving from CharacterMovementComponent velocity.
    // NPCs use SetActorLocation (server-driven), so GetVelocity() reflects the
    // last SetActorLocation delta computed by CMC — this is reliable enough for
    // blending between idle and walk states.
    const FVector Velocity = OwnerNPC->GetCharacterMovement()->Velocity;
    Speed     = Velocity.Size2D();   // horizontal speed only
    bIsMoving = Speed > 10.0f;
}

// ---------------------------------------------------------------------------
// PlayAction
// ---------------------------------------------------------------------------
bool UNPCAnimInstance::PlayAction(FName ActionSlug)
{
    UAnimMontage* const* Found = ActionMontageMap.Find(ActionSlug);
    if (!Found || !(*Found))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[NPCAnimInstance] PlayAction: no montage for slug '%s'"), *ActionSlug.ToString());
        return false;
    }
    return PlayMontageInternal(*Found);
}

// ---------------------------------------------------------------------------
// NotifyGreet
// ---------------------------------------------------------------------------
void UNPCAnimInstance::NotifyGreet()
{
    PlayAction(FName("greet"));
    OnGreet();          // Blueprint hook
}

// ---------------------------------------------------------------------------
// NotifyFarewell
// ---------------------------------------------------------------------------
void UNPCAnimInstance::NotifyFarewell()
{
    PlayAction(FName("farewell"));
    OnFarewell();       // Blueprint hook
}

// ---------------------------------------------------------------------------
// PickRandomIdleMontage
// Plays a randomly chosen entry from IdleMontages[] through the DefaultSlot
// and updates IdleVariantIndex for State Machine read-back.
// ---------------------------------------------------------------------------
void UNPCAnimInstance::PickRandomIdleMontage()
{
    if (IdleMontages.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[NPCAnimInstance] PickRandomIdleMontage: IdleMontages array is empty — assign AnimMontages in the AnimBlueprint defaults (NPC|Montages category)"));
        return;
    }

    const int32 Idx = FMath::RandRange(0, IdleMontages.Num() - 1);
    IdleVariantIndex = Idx;

    UAnimMontage* Montage = IdleMontages[Idx];
    if (Montage)
    {
        PlayMontageInternal(Montage);
    }
}

// ---------------------------------------------------------------------------
// CycleIdleVariant
// Increments IdleVariantIndex (wraps around) for State Machine-driven idles
// that don't use montages.
// ---------------------------------------------------------------------------
void UNPCAnimInstance::CycleIdleVariant()
{
    if (IdleMontages.Num() > 1)
    {
        IdleVariantIndex = (IdleVariantIndex + 1) % IdleMontages.Num();
    }
}

// ---------------------------------------------------------------------------
// SetTalking
// ---------------------------------------------------------------------------
void UNPCAnimInstance::SetTalking(bool bTalking)
{
    bIsTalking = bTalking;
}

// ---------------------------------------------------------------------------
// StopCurrentAction
// ---------------------------------------------------------------------------
void UNPCAnimInstance::StopCurrentAction(float BlendOutTime)
{
    if (ActiveActionMontage)
    {
        Montage_Stop(BlendOutTime, ActiveActionMontage);
        // ActiveActionMontage cleared in OnActionMontageEnded
    }
}

// ---------------------------------------------------------------------------
// PlayMontageInternal — play on the DefaultSlot and register end delegate
// ---------------------------------------------------------------------------
bool UNPCAnimInstance::PlayMontageInternal(UAnimMontage* Montage, float PlayRate)
{
    if (!Montage) return false;

    // Stop any currently playing action montage (blend out quickly)
    if (ActiveActionMontage && Montage_IsPlaying(ActiveActionMontage))
    {
        Montage_Stop(0.15f, ActiveActionMontage);
    }

    ActiveActionMontage = Montage;
    const float StartedAt = Montage_Play(Montage, PlayRate);
    if (StartedAt > 0.0f)
    {
        Montage_SetEndDelegate(ActionMontageEndedDelegate, Montage);
        return true;
    }
    UE_LOG(LogTemp, Warning,
        TEXT("[NPCAnimInstance] Montage_Play failed for '%s' — verify the AnimGraph has a 'DefaultSlot' node and the montage is compatible with this skeleton"),
        *Montage->GetName());    ActiveActionMontage = nullptr;
    return false;
}

// ---------------------------------------------------------------------------
// OnActionMontageEnded — clear active montage reference
// ---------------------------------------------------------------------------
void UNPCAnimInstance::OnActionMontageEnded(UAnimMontage* Montage, bool /*bInterrupted*/)
{
    if (ActiveActionMontage == Montage)
    {
        ActiveActionMontage = nullptr;
    }
}
