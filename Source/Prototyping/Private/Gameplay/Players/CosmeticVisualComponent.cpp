#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Data/CharacterCosmeticData.h"
#include "Data/CharacterVisualData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UCosmeticVisualComponent::UCosmeticVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCosmeticVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind from local equipment manager delegate
	if (BoundEquipmentManager)
	{
		BoundEquipmentManager->OnEquipmentStateChangedDelegate.RemoveDynamic(
			this, &UCosmeticVisualComponent::OnEquipmentStateChanged);
		BoundEquipmentManager = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UCosmeticVisualComponent::SetDefaultCosmetics(const FCharacterVisualDefinition& VisualDef,
                                                    UDataTable* InCosmeticsTable)
{
	CosmeticsTable = InCosmeticsTable;

	UE_LOG(LogTemp, Log, TEXT("[Cosmetic] SetDefaultCosmetics called. Table=%s  HairSlug='%s'  FacialSlug='%s'"),
		InCosmeticsTable ? *InCosmeticsTable->GetName() : TEXT("NULL — assign CharacterCosmeticsDataTable in GameInstance BP"),
		*VisualDef.DefaultHairSlug.ToString(),
		*VisualDef.DefaultFacialHairSlug.ToString());

	if (!CosmeticsTable)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Cosmetic] ABORT — CharacterCosmeticsDataTable is null. Open BP_GameInstance and assign the table."));
		return;
	}

	if (VisualDef.DefaultHairSlug == NAME_None && VisualDef.DefaultFacialHairSlug == NAME_None)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Cosmetic] Both DefaultHairSlug and DefaultFacialHairSlug are None — nothing to spawn. "
			     "Fill them in DT_CharacterVisualDefinitions for this class+race+gender row."));
	}

	if (VisualDef.DefaultHairSlug != NAME_None)
	{
		SpawnCosmeticMesh(VisualDef.DefaultHairSlug);
	}

	if (VisualDef.DefaultFacialHairSlug != NAME_None)
	{
		SpawnCosmeticMesh(VisualDef.DefaultFacialHairSlug);
	}

	// Re-evaluate in case equipment state was applied before cosmetics were spawned
	EvaluateAllVisibility();
}

void UCosmeticVisualComponent::BindEquipmentManager(UEquipmentManager* InEquipmentManager)
{
	// Unbind from previous manager if any
	if (BoundEquipmentManager)
	{
		BoundEquipmentManager->OnEquipmentStateChangedDelegate.RemoveDynamic(
			this, &UCosmeticVisualComponent::OnEquipmentStateChanged);
	}

	BoundEquipmentManager = InEquipmentManager;

	if (BoundEquipmentManager)
	{
		BoundEquipmentManager->OnEquipmentStateChangedDelegate.AddDynamic(
			this, &UCosmeticVisualComponent::OnEquipmentStateChanged);

		// Apply the manager's current state immediately so visibility is correct at spawn
		ApplyEquipmentState(BoundEquipmentManager->GetEquipmentState());
	}
}

void UCosmeticVisualComponent::ApplyEquipmentState(const FEquipmentStateData& State)
{
	OccupiedEquipSlots.Reset();
	for (const auto& Pair : State.slots)
	{
		if (Pair.Value.bIsOccupied)
		{
			OccupiedEquipSlots.Add(FName(*Pair.Key));
		}
	}
	EvaluateAllVisibility();
}

void UCosmeticVisualComponent::HandleRemoteEquipmentState(const FEquipmentStateData& State)
{
	if (OwnerCharacterId <= 0 || State.characterId != OwnerCharacterId)
	{
		return;
	}
	ApplyEquipmentState(State);
}

void UCosmeticVisualComponent::SetCosmeticSlot(FName CosmeticSlot, FName CosmeticSlug)
{
	if (!CosmeticsTable)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CosmeticVisualComponent: SetCosmeticSlot called before SetDefaultCosmetics — "
			     "CharacterCosmeticsDataTable must be set first"));
		return;
	}

	DestroySlotComponent(CosmeticSlot);

	if (CosmeticSlug != NAME_None)
	{
		SpawnCosmeticMesh(CosmeticSlug);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void UCosmeticVisualComponent::SpawnCosmeticMesh(FName CosmeticSlug)
{
	if (!CosmeticsTable || CosmeticSlug == NAME_None) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	UE_LOG(LogTemp, Log, TEXT("[Cosmetic] SpawnCosmeticMesh: slug='%s' owner='%s'"),
		*CosmeticSlug.ToString(), *Owner->GetName());

	const FCharacterCosmeticData* CosmeticData =
		CosmeticsTable->FindRow<FCharacterCosmeticData>(CosmeticSlug, TEXT("CosmeticVisual"));

	if (!CosmeticData)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Cosmetic] No row '%s' found in CharacterCosmeticsDataTable"),
			*CosmeticSlug.ToString());
		return;
	}

	const FName SlotName = CosmeticData->CosmeticSlot;
	if (SlotName == NAME_None)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Cosmetic] Row '%s' has empty CosmeticSlot field"),
			*CosmeticSlug.ToString());
		return;
	}

	const bool bUseStatic = !CosmeticData->StaticMesh.IsNull();
	UE_LOG(LogTemp, Log, TEXT("[Cosmetic] slug='%s' slot='%s' type=%s  staticPath='%s'  skelPath='%s'"),
		*CosmeticSlug.ToString(),
		*SlotName.ToString(),
		bUseStatic ? TEXT("StaticMesh") : TEXT("SkeletalMesh"),
		*CosmeticData->StaticMesh.ToSoftObjectPath().ToString(),
		*CosmeticData->SkeletalMesh.ToSoftObjectPath().ToString());

	// Remove whatever was previously in this slot
	DestroySlotComponent(SlotName);

	// Cache hide rules before creating the component so EvaluateAllVisibility can use them
	SlugToHideRules.Add(CosmeticSlug, CosmeticData->HideWhenEquipSlotsOccupied);
	SlotToSlug.Add(SlotName, CosmeticSlug);

	// Find body SkeletalMeshComponent — needed by both branches
	USkeletalMeshComponent* BodyMesh = nullptr;
	{
		TArray<USkeletalMeshComponent*> AllSkelMeshes;
		Owner->GetComponents<USkeletalMeshComponent>(AllSkelMeshes);
		if (AllSkelMeshes.Num() > 0)
		{
			BodyMesh = AllSkelMeshes[0];
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Cosmetic] BodyMesh found: %s  (has skeletal mesh asset: %s)"),
		BodyMesh ? TEXT("YES") : TEXT("NO — cosmetic will not follow skeleton"),
		(BodyMesh && BodyMesh->GetSkeletalMeshAsset()) ? TEXT("YES") : TEXT("NO — body mesh asset still loading, socket attach will activate once it loads"));

	if (bUseStatic)
	{
		// ── StaticMesh branch — socket attachment ─────────────────────────────
		UStaticMeshComponent* NewComp = NewObject<UStaticMeshComponent>(Owner,
			*FString::Printf(TEXT("Cosmetic_SM_%s_%d"), *SlotName.ToString(), FMath::Rand()));

		NewComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// SetupAttachment BEFORE RegisterComponent — the correct UE pattern for
		// runtime-created components. Registering unattached causes a stale
		// world-space transform that can persist after the subsequent AttachToComponent.
		if (BodyMesh && CosmeticData->AttachSocketName != NAME_None)
		{
			UE_LOG(LogTemp, Log, TEXT("[Cosmetic] StaticMesh: attaching to socket '%s' on body mesh"),
				*CosmeticData->AttachSocketName.ToString());
			NewComp->SetupAttachment(BodyMesh, CosmeticData->AttachSocketName);
		}
		else if (BodyMesh)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Cosmetic] Row '%s' — StaticMesh set but AttachSocketName is None, attaching to mesh root"),
				*CosmeticSlug.ToString());
			NewComp->SetupAttachment(BodyMesh);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Cosmetic] Owner '%s' has no body SkeletalMeshComponent — StaticMesh '%s' attached to actor root"),
				*Owner->GetName(), *CosmeticSlug.ToString());
			NewComp->SetupAttachment(Owner->GetRootComponent());
		}

		if (!CosmeticData->StaticMeshRelativeTransform.Equals(FTransform::Identity))
		{
			NewComp->SetRelativeTransform(CosmeticData->StaticMeshRelativeTransform);
		}

		NewComp->RegisterComponent();
		Owner->AddInstanceComponent(NewComp);

		CosmeticMeshes.Add(SlotName, NewComp);

		UE_LOG(LogTemp, Log, TEXT("[Cosmetic] StaticMeshComponent created and registered. Starting async load..."));

		TWeakObjectPtr<UStaticMeshComponent> WeakComp(NewComp);
		TSoftObjectPtr<UStaticMesh> SoftMesh = CosmeticData->StaticMesh;
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath(),
			[WeakComp, SoftMesh, CosmeticSlug]()
			{
				if (!WeakComp.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("[Cosmetic] Async load done for '%s' but component is no longer valid"),
						*CosmeticSlug.ToString());
					return;
				}
				UStaticMesh* LoadedMesh = SoftMesh.Get();
				if (LoadedMesh)
				{
					UE_LOG(LogTemp, Log, TEXT("[Cosmetic] Async load SUCCESS for '%s': mesh='%s' visible=%s"),
						*CosmeticSlug.ToString(),
						*LoadedMesh->GetName(),
						WeakComp->IsVisible() ? TEXT("YES") : TEXT("NO"));
					WeakComp->SetStaticMesh(LoadedMesh);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[Cosmetic] Async load FAILED for '%s' — asset path may be wrong or asset deleted"),
						*CosmeticSlug.ToString());
				}
			});
	}
	else
	{
		// ── SkeletalMesh branch — Leader Pose ────────────────────────────────
		USkeletalMeshComponent* NewComp = NewObject<USkeletalMeshComponent>(Owner,
			*FString::Printf(TEXT("Cosmetic_SK_%s_%d"), *SlotName.ToString(), FMath::Rand()));

		NewComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Use leader's bounds for frustum culling — avoids culling when AABB is uninitialized
		NewComp->bUseBoundsFromLeaderPoseComponent = true;

		// SetupAttachment BEFORE RegisterComponent
		USceneComponent* AttachTarget = BodyMesh ? Cast<USceneComponent>(BodyMesh) : Owner->GetRootComponent();
		NewComp->SetupAttachment(AttachTarget);

		NewComp->RegisterComponent();
		Owner->AddInstanceComponent(NewComp);

		if (BodyMesh)
		{
			NewComp->SetLeaderPoseComponent(BodyMesh);
			UE_LOG(LogTemp, Log, TEXT("[Cosmetic] SkeletalMesh: Leader Pose set to body mesh '%s'"),
				*BodyMesh->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Cosmetic] Owner '%s' has no body SkeletalMeshComponent — cosmetic '%s' will not animate"),
				*Owner->GetName(), *CosmeticSlug.ToString());
		}

		CosmeticMeshes.Add(SlotName, NewComp);

		if (!CosmeticData->SkeletalMesh.IsNull())
		{
			TWeakObjectPtr<USkeletalMeshComponent> WeakComp(NewComp);
			TSoftObjectPtr<USkeletalMesh> SoftMesh = CosmeticData->SkeletalMesh;
			FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
			Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath(),
				[WeakComp, SoftMesh, CosmeticSlug]()
				{
					if (!WeakComp.IsValid())
					{
						UE_LOG(LogTemp, Warning, TEXT("[Cosmetic] Async load done for '%s' but component is no longer valid"),
							*CosmeticSlug.ToString());
						return;
					}
					USkeletalMesh* LoadedMesh = SoftMesh.Get();
					if (LoadedMesh)
					{
						UE_LOG(LogTemp, Log, TEXT("[Cosmetic] Async load SUCCESS for '%s': mesh='%s' visible=%s"),
							*CosmeticSlug.ToString(),
							*LoadedMesh->GetName(),
							WeakComp->IsVisible() ? TEXT("YES") : TEXT("NO"));
						WeakComp->SetSkeletalMesh(LoadedMesh);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[Cosmetic] Async load FAILED for '%s' — asset path may be wrong or asset deleted"),
							*CosmeticSlug.ToString());
					}
				});
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Cosmetic] Row '%s' has null SkeletalMesh and null StaticMesh — assign at least one mesh"),
				*CosmeticSlug.ToString());
		}
	}

	// Apply current visibility state to the freshly spawned component
	EvaluateAllVisibility();
}

void UCosmeticVisualComponent::DestroySlotComponent(FName CosmeticSlot)
{
	if (USceneComponent** Found = CosmeticMeshes.Find(CosmeticSlot))
	{
		if (IsValid(*Found))
		{
			(*Found)->DestroyComponent();
		}
		CosmeticMeshes.Remove(CosmeticSlot);
	}

	// Clean up cached data for the slug that was in this slot
	if (const FName* OldSlug = SlotToSlug.Find(CosmeticSlot))
	{
		SlugToHideRules.Remove(*OldSlug);
	}
	SlotToSlug.Remove(CosmeticSlot);
}

void UCosmeticVisualComponent::EvaluateAllVisibility()
{
	for (auto& Pair : CosmeticMeshes)
	{
		const FName& SlotName = Pair.Key;
		USceneComponent* Comp = Pair.Value;
		if (!IsValid(Comp)) continue;

		const FName* Slug = SlotToSlug.Find(SlotName);
		if (!Slug) continue;

		const TArray<FName>* HideRules = SlugToHideRules.Find(*Slug);
		bool bShouldHide = false;

		if (HideRules)
		{
			for (const FName& EquipSlot : *HideRules)
			{
				if (OccupiedEquipSlots.Contains(EquipSlot))
				{
					bShouldHide = true;
					break;
				}
			}
		}

		Comp->SetVisibility(!bShouldHide);
	}
}

void UCosmeticVisualComponent::OnEquipmentStateChanged(const FEquipmentStateData& State)
{
	ApplyEquipmentState(State);
}
