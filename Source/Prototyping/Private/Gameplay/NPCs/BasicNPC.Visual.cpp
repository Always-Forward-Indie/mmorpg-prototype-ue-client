#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/NPCs/NPCAnimInstance.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/UI/NPCNameplateComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Data/EntityAudioRepository.h"
#include "GameFramework/Pawn.h"

void ABasicNPC::SetupNPCVisual(FName NPCSlug)
{
	if (!NPCDefinitionTable) { UE_LOG(LogTemp, Warning, TEXT("No NPCDefinitionTable")); return; }
	const FNPCDefinition* Def = NPCDefinitionTable->FindRow<FNPCDefinition>(NPCSlug, TEXT("Load NPC Definition"));
	if (!Def) { UE_LOG(LogTemp, Warning, TEXT("No row for %s"), *NPCSlug.ToString()); return; }

	const auto SkeletalMeshSoft = Def->Visual.SkeletalMesh;
	const auto AnimBPSoft = Def->Visual.AnimBPClass;
	SetActorScale3D(Def->Visual.ActorScale);

	// Snapshot the visual data before entering async lambdas —
	// the DataTable row pointer is only safe on this stack frame.
	const FNPCVisualData VisualDataCopy = Def->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<ABasicNPC> WeakThis(this);

	if (!SkeletalMeshSoft.IsNull())
	{
		Streamable.RequestAsyncLoad(SkeletalMeshSoft.ToSoftObjectPath(), [WeakThis, SkeletalMeshSoft]()
			{
				ABasicNPC* Self = WeakThis.Get();
				if (!Self) { return; }

				if (USkeletalMesh* Mesh = SkeletalMeshSoft.Get())
				{
					USkeletalMeshComponent* MC = Self->GetMesh();
					if (MC) {
						MC->SetSkeletalMesh(Mesh);

						const FBoxSphereBounds B = Mesh->GetBounds();
						if (UCapsuleComponent* Cap = Self->GetCapsuleComponent())
						{
							const float CapsuleRadius = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y);
							const float CapsuleHalf = B.BoxExtent.Z;
							Cap->SetCapsuleRadius(CapsuleRadius);
							Cap->SetCapsuleHalfHeight(CapsuleHalf);

							MC->SetRelativeLocation(FVector(0, 0, -Cap->GetUnscaledCapsuleHalfHeight()));

							// Re-snap after the capsule is correctly sized.
							// The initial SnapToGround() used the default ACharacter capsule,
							// so the actor Z needs to be corrected now that we know the real height.
							Self->SnapToGround();
						}
					}
				}
			});
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("SkeletalMesh is not set for slug %s"), *NPCSlug.ToString());
	}

	if (!AnimBPSoft.IsNull())
	{
		Streamable.RequestAsyncLoad(AnimBPSoft.ToSoftObjectPath(), [WeakThis, AnimBPSoft, VisualDataCopy]()
			{
				ABasicNPC* Self = WeakThis.Get();
				if (!Self) { return; }

				if (UClass* AnimClass = AnimBPSoft.Get())
				{
					if (USkeletalMeshComponent* MC = Self->GetMesh())
					{
						MC->SetAnimInstanceClass(AnimClass);

						// --- Populate montage assets on BasicNPC directly ---
						// Using ACharacter::PlayAnimMontage() so playback works
						// regardless of whether the ABP inherits from UNPCAnimInstance.
						if (!VisualDataCopy.GreetMontage.IsNull())
							Self->GreetMontageAsset = VisualDataCopy.GreetMontage.LoadSynchronous();

						if (!VisualDataCopy.FarewellMontage.IsNull())
							Self->FarewellMontageAsset = VisualDataCopy.FarewellMontage.LoadSynchronous();

						Self->IdleMontageAssets.Reset();
						for (const auto& Soft : VisualDataCopy.IdleMontages)
							if (!Soft.IsNull())
								if (UAnimMontage* M = Soft.LoadSynchronous())
									Self->IdleMontageAssets.Add(M);

						if (!VisualDataCopy.DefaultIdleMontage.IsNull())
							Self->DefaultIdleMontageAsset = VisualDataCopy.DefaultIdleMontage.LoadSynchronous();

						UE_LOG(LogTemp, Log,
							TEXT("[NPCAnim] Montages loaded for '%s': greet=%s farewell=%s defaultIdle=%s idleCount=%d"),
							*Self->GetNPCName(),
							Self->GreetMontageAsset        ? *Self->GreetMontageAsset->GetName()        : TEXT("none"),
							Self->FarewellMontageAsset     ? *Self->FarewellMontageAsset->GetName()     : TEXT("none"),
							Self->DefaultIdleMontageAsset  ? *Self->DefaultIdleMontageAsset->GetName()  : TEXT("none"),
							Self->IdleMontageAssets.Num());

						// --- Also populate UNPCAnimInstance maps if the ABP uses it ---
						// (allows bIsTalking / Speed state-machine integration)
						if (UNPCAnimInstance* Anim = Self->GetNPCAnimInstance())
						{
							if (Self->GreetMontageAsset)
								Anim->ActionMontageMap.Add(FName("greet"), Self->GreetMontageAsset);
							if (Self->FarewellMontageAsset)
								Anim->ActionMontageMap.Add(FName("farewell"), Self->FarewellMontageAsset);
							Anim->IdleMontages = Self->IdleMontageAssets;
						}

						// Start idle animation cycle — plays DefaultIdleMontage immediately if set
						Self->StartIdleAnimCycle();
					}
				}
			});
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AnimBPClass is not set for slug %s"), *NPCSlug.ToString());
	}
}

void ABasicNPC::StartIdleAnimCycle()
{
	// If a DefaultIdleMontage is assigned, play it right now so the NPC is never
	// frozen in a T-pose waiting for the first random timer to fire.
	// OnIdleMontageEnded will chain into ScheduleNextIdleAnim() afterwards.
	if (DefaultIdleMontageAsset && !bIdleAnimPlaying)
	{
		const float Duration = PlayAnimMontage(DefaultIdleMontageAsset);
		if (Duration > 0.f)
		{
			ActiveIdleMontage = DefaultIdleMontageAsset;
			bIdleAnimPlaying  = true;

			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				IdleEndedDelegate.BindUObject(this, &ABasicNPC::OnIdleMontageEnded);
				AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, DefaultIdleMontageAsset);
			}
			UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing DefaultIdleMontage immediately: '%s'"),
				*GetNPCName(), *DefaultIdleMontageAsset->GetName());
			return;
		}
	}
	// No DefaultIdleMontage (or playback failed) — fall through to the timed cycle.
	ScheduleNextIdleAnim();
}

void ABasicNPC::ScheduleNextIdleAnim()
{
	if (!GetWorld()) return;
	// 8-20 seconds between random idle variants — visible during normal gameplay
	const float Delay = FMath::FRandRange(8.0f, 20.0f);
	GetWorld()->GetTimerManager().SetTimer(IdleAnimTimerHandle, this, &ABasicNPC::TriggerRandomIdleAnim, Delay, false);
}

void ABasicNPC::TriggerRandomIdleAnim()
{
	// If an idle is already playing let it finish — the end delegate reschedules.
	if (bIdleAnimPlaying)
		return;

	if (IdleMontageAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NPCAnim] '%s' has no IdleMontages — assign them in the NPCDefinition DataTable (Visual.IdleMontages)"),
			*GetNPCName());
		return; // nothing to play, don't reschedule
	}

	const int32 Idx  = FMath::RandRange(0, IdleMontageAssets.Num() - 1);
	UAnimMontage* Montage = IdleMontageAssets[Idx];
	if (!Montage) { ScheduleNextIdleAnim(); return; }

	const float Duration = PlayAnimMontage(Montage);
	if (Duration > 0.f)
	{
		ActiveIdleMontage = Montage;
		bIdleAnimPlaying   = true;

		// Bind end delegate — fires when the montage finishes or is stopped
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			IdleEndedDelegate.BindUObject(this, &ABasicNPC::OnIdleMontageEnded);
			AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, Montage);
		}
		UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing idle variant %d: '%s'"),
			*GetNPCName(), Idx, *Montage->GetName());
	}
	else
	{
		// Playback failed (e.g. no DefaultSlot in AnimGraph) — fall back to timer
		ScheduleNextIdleAnim();
	}

	// Keep UNPCAnimInstance IdleVariantIndex in sync for State Machine read-back
	if (UNPCAnimInstance* Anim = GetNPCAnimInstance())
		Anim->IdleVariantIndex = Idx;
}

void ABasicNPC::OnIdleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ActiveIdleMontage != Montage) return;
	ActiveIdleMontage = nullptr;
	bIdleAnimPlaying  = false;

	// Only reschedule the idle cycle when the montage finished naturally.
	// If it was interrupted by greet/farewell those methods bind their own
	// ActionEndedDelegate and reschedule after the action montage ends.
	if (!bInterrupted)
	{
		ScheduleNextIdleAnim();
	}
}

void ABasicNPC::OnActionMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
	// After a greeting or farewell montage ends, resume the idle animation cycle.
	if (!bInterrupted)
	{
		ScheduleNextIdleAnim();
	}
}

