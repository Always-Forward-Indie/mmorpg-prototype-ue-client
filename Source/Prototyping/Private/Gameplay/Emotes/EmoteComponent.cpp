#include "Gameplay/Emotes/EmoteComponent.h"
#include "Data/EmoteDefinitionTable.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

UEmoteComponent::UEmoteComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEmoteComponent::BeginPlay()
{
    Super::BeginPlay();
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void UEmoteComponent::PlayEmoteBySlug(const FString& EmoteSlug, const FString& AnimationName)
{
    if (EmoteSlug.IsEmpty()) return;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar)
    {
        UE_LOG(LogTemp, Warning, TEXT("EmoteComponent: Owner is not an ACharacter — cannot play emote '%s'"), *EmoteSlug);
        return;
    }

    // Stop any running emote first
    if (bIsPlaying)
    {
        StopCurrentEmote();
    }

    const FEmoteTableRow* Row = FindTableRow(EmoteSlug);
    if (!Row)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("EmoteComponent: No DataTable row for emote slug '%s'. "
                 "Add a row to DT_EmoteDefinitions. AnimationName hint: '%s'"),
            *EmoteSlug, *AnimationName);
        return;
    }

    // Load montage synchronously (soft-ref; assets should be in memory already for a limited emote set)
    UAnimMontage* Montage = Row->EmoteMontage.LoadSynchronous();
    if (!Montage)
    {
        UE_LOG(LogTemp, Warning, TEXT("EmoteComponent: EmoteMontage not set in DT row for '%s'"), *EmoteSlug);
        return;
    }

    // Bind montage-ended delegate
    UAnimInstance* AnimInst = OwnerChar->GetMesh() ? OwnerChar->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInst)
    {
        UE_LOG(LogTemp, Warning, TEXT("EmoteComponent: No AnimInstance on '%s'"), *GetOwner()->GetName());
        return;
    }

    const float Duration = OwnerChar->PlayAnimMontage(Montage, Row->PlayRate);
    if (Duration <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("EmoteComponent: PlayAnimMontage returned 0 for '%s' — check slot assignment"), *EmoteSlug);
        return;
    }

    AnimInst->OnMontageEnded.RemoveDynamic(this, &UEmoteComponent::OnMontageEnded);
    AnimInst->OnMontageEnded.AddDynamic(this, &UEmoteComponent::OnMontageEnded);

    // Cache state
    CurrentMontageAsset         = Montage;
    CurrentEmoteSlug            = EmoteSlug;
    bIsPlaying                  = true;
    bCurrentInterruptOnDmg      = Row->bInterruptOnDamage;
    bCurrentInterruptOnMov      = Row->bInterruptOnMove;

    // Determine whether the montage has our custom notifies (peek notify tracks)
    bHasSoundNotify = false;
    bHasVFXNotify   = false;
    for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
    {
        if (const UAnimNotify* Notify = NotifyEvent.Notify.Get())
        {
            const FString ClassName = Notify->GetClass()->GetName();
            if (ClassName == TEXT("AnimNotify_EmoteEvent"))
            {
                // We know at least one exists — check slot via cast
                // Use soft approach: the AnimNotify_EmoteEvent header is not included here
                // to avoid a circular include; we rely on the class name string.
                // The actual dispatch happens in the notify itself.
                bHasSoundNotify = true;
                bHasVFXNotify   = true;
                break;
            }
        }
    }

    // Fallback: fire sound and VFX immediately if there are no notifies in the montage
    if (!bHasSoundNotify && !Row->EmoteSound.IsNull())
    {
        ExecutePlaySound(*Row);
    }
    if (!bHasVFXNotify && !Row->EmoteVFX.IsNull())
    {
        ExecuteSpawnVFX(*Row);
    }

    OnEmoteStarted.Broadcast(EmoteSlug);
    UE_LOG(LogTemp, Log, TEXT("EmoteComponent: Playing emote '%s' on '%s'"), *EmoteSlug, *GetOwner()->GetName());
}

void UEmoteComponent::StopCurrentEmote()
{
    if (!bIsPlaying || CurrentEmoteSlug.IsEmpty()) return;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && CurrentMontageAsset)
    {
        OwnerChar->StopAnimMontage(CurrentMontageAsset);
    }

    // Destroy any looping VFX
    if (ActiveVFXComponent && IsValid(ActiveVFXComponent))
    {
        ActiveVFXComponent->DeactivateImmediate();
        ActiveVFXComponent = nullptr;
    }

    ClearEmoteState(true);
}

// ---------------------------------------------------------------------------
// Interruption hooks
// ---------------------------------------------------------------------------

void UEmoteComponent::NotifyMovementStarted()
{
    if (bIsPlaying && bCurrentInterruptOnMov)
    {
        StopCurrentEmote();
    }
}

void UEmoteComponent::NotifyDamageReceived()
{
    if (bIsPlaying && bCurrentInterruptOnDmg)
    {
        StopCurrentEmote();
    }
}

// ---------------------------------------------------------------------------
// AnimNotify callbacks
// ---------------------------------------------------------------------------

void UEmoteComponent::OnEmoteNotify_PlaySound()
{
    if (!bIsPlaying || CurrentEmoteSlug.IsEmpty()) return;
    const FEmoteTableRow* Row = FindTableRow(CurrentEmoteSlug);
    if (Row) ExecutePlaySound(*Row);
}

void UEmoteComponent::OnEmoteNotify_SpawnVFX()
{
    if (!bIsPlaying || CurrentEmoteSlug.IsEmpty()) return;
    const FEmoteTableRow* Row = FindTableRow(CurrentEmoteSlug);
    if (Row) ExecuteSpawnVFX(*Row);
}

void UEmoteComponent::OnEmoteNotify_EmoteEnd()
{
    if (!bIsPlaying) return;

    // Destroy VFX on explicit end signal
    if (ActiveVFXComponent && IsValid(ActiveVFXComponent))
    {
        ActiveVFXComponent->DeactivateImmediate();
        ActiveVFXComponent = nullptr;
    }

    ClearEmoteState(false);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

const FEmoteTableRow* UEmoteComponent::FindTableRow(const FString& Slug) const
{
    if (!EmoteDefinitionTable) return nullptr;
    return EmoteDefinitionTable->FindRow<FEmoteTableRow>(FName(*Slug), TEXT("EmoteComponent"), false);
}

void UEmoteComponent::ExecutePlaySound(const FEmoteTableRow& Row) const
{
    if (Row.EmoteSound.IsNull()) return;

    USoundBase* Sound = Row.EmoteSound.LoadSynchronous();
    if (!Sound) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    UGameplayStatics::SpawnSoundAttached(Sound, Owner->GetRootComponent(),
        NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
        false, 1.f, 1.f, 0.f, nullptr, nullptr, true);
}

void UEmoteComponent::ExecuteSpawnVFX(const FEmoteTableRow& Row)
{
    if (Row.EmoteVFX.IsNull()) { return; }

    UNiagaraSystem* VFXSystem = Row.EmoteVFX.LoadSynchronous();
    if (!VFXSystem) { return; }

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar || !OwnerChar->GetMesh()) { return; }

    const FName AttachSocket = Row.VFXSocketName.IsNone() ? FName("root") : Row.VFXSocketName;

    UNiagaraComponent* SpawnedComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
        VFXSystem,
        OwnerChar->GetMesh(),
        AttachSocket,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::SnapToTargetIncludingScale,
        /*bAutoDestroy=*/ !Row.bLooping);

    if (SpawnedComp && Row.bLooping)
    {
        // Keep reference for manual cleanup on looping emotes
        ActiveVFXComponent = SpawnedComp;
    }
}

void UEmoteComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != CurrentMontageAsset) { return; }

    // Destroy any looping VFX
    if (ActiveVFXComponent && IsValid(ActiveVFXComponent))
    {
        ActiveVFXComponent->DeactivateImmediate();
        ActiveVFXComponent = nullptr;
    }

    ClearEmoteState(bInterrupted);
}

void UEmoteComponent::ClearEmoteState(bool bInterrupted)
{
    if (!bIsPlaying) return;

    const FString FinishedSlug = CurrentEmoteSlug;
    CurrentEmoteSlug  = TEXT("");
    bIsPlaying        = false;
    CurrentMontageAsset = nullptr;

    OnEmoteEnded.Broadcast(FinishedSlug);
    UE_LOG(LogTemp, Log, TEXT("EmoteComponent: Emote '%s' ended (interrupted: %s)"),
        *FinishedSlug, bInterrupted ? TEXT("yes") : TEXT("no"));
}
