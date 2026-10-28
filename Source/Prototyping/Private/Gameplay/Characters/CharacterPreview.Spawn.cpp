// Character Preview Manager — Implementation

#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/CharacterVisualData.h"
#include "Data/DataStructs.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

ABasicPlayer* UCharacterPreviewManager::SpawnPreviewActor(const FTransform& Transform)
{
	if (!GameInstanceRef || !GameInstanceRef->MainPlayerClass || !GameInstanceRef->GetWorld())
		return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABasicPlayer* Actor = GameInstanceRef->GetWorld()->SpawnActorDeferred<ABasicPlayer>(
		GameInstanceRef->MainPlayerClass, Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Actor) return nullptr;

	// Mark as "other client" so it doesn't create HUD/input bindings
	Actor->SetIsOtherClient(true);

	UGameplayStatics::FinishSpawningActor(Actor, Transform);

	// Keep capsule query-only so click line traces (ECC_Visibility) can hit it.
	// No physics response, no blocking of movement — purely for trace detection.
	if (UCapsuleComponent* Capsule = Actor->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Disable movement completely — SetMovementMode(MOVE_None) stops gravity from
	// overriding our SetActorLocation calls. GravityScale=0 is belt-and-suspenders.
	if (UCharacterMovementComponent* MoveComp = Actor->GetCharacterMovement())
	{
		MoveComp->GravityScale = 0.f;
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	return Actor;
}

void UCharacterPreviewManager::ApplyVisualToActor(ABasicPlayer* Actor, const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName)
{
	if (!Actor) return;

	UDataTable* Table = GetVisualTable();
	if (!Table)
	{
		UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: No CharacterVisualDefinitionsTable set on GameInstance"));
		return;
	}

	const FCharacterVisualDefinition* Def = nullptr;
	FName MatchedKey = NAME_None;
	Def = CharacterVisualHelper::FindVisualDefinition(
		Table, ClassSlug, RaceSlug, GenderName, &MatchedKey);

	if (!Def)
	{
		UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: No visual definition found for %s_%s_%s"),
			*ClassSlug, *RaceSlug, *GenderName);
		return;
	}

	if (!GenderName.IsEmpty()
		&& MatchedKey != CharacterVisualHelper::MakeRowKey(ClassSlug, RaceSlug, GenderName))
	{
		UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: gender '%s' resolved to fallback row '%s' (exact %s_%s_%s missing)"),
			*GenderName, *MatchedKey.ToString(), *ClassSlug, *RaceSlug, *GenderName);
	}

	const FCharacterVisualData& VisualData = Def->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	// Async load skeletal mesh
	TSoftObjectPtr<USkeletalMesh> SoftMesh = VisualData.SkeletalMesh;
	TSoftClassPtr<UAnimInstance> SoftAnimBP = VisualData.AnimBPClass;

	TWeakObjectPtr<ABasicPlayer> WeakActor(Actor);

	if (!SoftMesh.IsNull())
	{
		Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath(), [WeakActor, SoftMesh, MatchedKey]()
		{
			if (!WeakActor.IsValid()) return;
			if (USkeletalMesh* LoadedMesh = SoftMesh.Get())
			{
				USkeletalMeshComponent* MeshComp = WeakActor->GetMesh();
				if (MeshComp)
				{
					MeshComp->SetSkeletalMesh(LoadedMesh);
					MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

					// Auto-fit capsule like mobs do
					const FBoxSphereBounds MeshBounds = LoadedMesh->GetBounds();
					const FVector BoxExtent = MeshBounds.BoxExtent;
					const FVector MeshOrigin = MeshBounds.Origin;

					UCapsuleComponent* Capsule = WeakActor->GetCapsuleComponent();
					if (Capsule)
					{
						float CapsuleHalfHeight = BoxExtent.Z;
						float MeshBottom = MeshOrigin.Z - BoxExtent.Z;
						float CapsuleBottom = -Capsule->GetUnscaledCapsuleHalfHeight();
						float MeshOffset = CapsuleBottom - MeshBottom;
						MeshComp->SetRelativeLocation(FVector(0, 0, MeshOffset));
						MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
					}
				}
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: visual mesh failed to load for row '%s'"),
					*MatchedKey.ToString());
			}
		});
	}

	if (!SoftAnimBP.IsNull())
	{
		Streamable.RequestAsyncLoad(SoftAnimBP.ToSoftObjectPath(), [WeakActor, SoftAnimBP, MatchedKey]()
		{
			if (!WeakActor.IsValid()) return;
			if (UClass* AnimClass = SoftAnimBP.Get())
			{
				WeakActor->GetMesh()->SetAnimInstanceClass(AnimClass);
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: AnimBP failed to load for row '%s'"),
					*MatchedKey.ToString());
			}
		});
	}

	// Apply scale
	Actor->SetActorScale3D(VisualData.ActorScale);

	// Initialize default cosmetics (hair etc.) from the visual definition.
	// SetLeaderPoseComponent does not require the body mesh asset to be loaded yet.
	UCosmeticVisualComponent* CosmeticVis = Actor->GetCosmeticVisualComponent();
	UDataTable* CosmeticsTable = GameInstanceRef ? GameInstanceRef->CharacterCosmeticsDataTable : nullptr;
	UE_LOG(LogGameplay, Verbose,
		TEXT("[Cosmetic] PreviewManager::ApplyVisualToActor — CosmeticVis=%s  GameInstanceRef=%s  CosmeticsTable=%s  HairSlug='%s'"),
		CosmeticVis ? TEXT("OK") : TEXT("NULL — CosmeticVisualComponent not found on actor (recompile + resave BP?)"),
		GameInstanceRef ? TEXT("OK") : TEXT("NULL"),
		CosmeticsTable ? *CosmeticsTable->GetName() : TEXT("NULL — assign CharacterCosmeticsDataTable in BP_GameInstance"),
		*Def->DefaultHairSlug.ToString());
	if (CosmeticVis)
	{
		CosmeticVis->SetDefaultCosmetics(*Def, CosmeticsTable);
	}
}

UDataTable* UCharacterPreviewManager::GetVisualTable() const
{
	return GameInstanceRef ? GameInstanceRef->CharacterVisualDefinitionsTable : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Equipment preview
// ─────────────────────────────────────────────────────────────────────────────

FString UCharacterPreviewManager::SlotIdToSlug(int32 SlotId)
{
	// Matches equip_slot table in DB (mmo_prototype_dump.sql)
	switch (SlotId)
	{
	case 1:  return TEXT("head");
	case 2:  return TEXT("chest");
	case 3:  return TEXT("legs");
	case 4:  return TEXT("feet");
	case 5:  return TEXT("hands");
	case 6:  return TEXT("main_hand");
	case 7:  return TEXT("off_hand");
	case 8:  return TEXT("two_hand");
	case 9:  return TEXT("ring");
	case 10: return TEXT("neck");
	case 11: return TEXT("trinket");
	default: return TEXT("");
	}
}

void UCharacterPreviewManager::ApplyEquipmentToActor(ABasicPlayer* Actor, const TArray<FLoginEquipmentEntry>& Equipment)
{
	if (!Actor || Equipment.IsEmpty()) return;

	UEquipmentVisualComponent* EquipVis = Actor->GetEquipmentVisualComponent();
	if (!EquipVis) return;

	UItemManager* ItemMgr = GameInstanceRef ? GameInstanceRef->GetItemManager() : nullptr;
	if (!ItemMgr)
	{
		UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: ItemManager not available — cannot apply equipment preview"));
		return;
	}

	EquipVis->InitializeForRemotePlayer(ItemMgr);

	// Build FEquipmentStateData from the lightweight preview entries
	FEquipmentStateData StateData;
	StateData.characterId = 0; // preview chars have no real ID

	for (const FLoginEquipmentEntry& Entry : Equipment)
	{
		const FString SlotSlug = SlotIdToSlug(Entry.SlotId);
		if (SlotSlug.IsEmpty())
		{
			UE_LOG(LogGameplay, Warning, TEXT("CharacterPreviewManager: Unknown slotId %d for item '%s'"), Entry.SlotId, *Entry.ItemSlug);
			continue;
		}

		FEquipmentSlotData SlotData;
		SlotData.slotSlug   = SlotSlug;
		SlotData.itemSlug   = Entry.ItemSlug;
		SlotData.bIsOccupied = true;
		StateData.slots.Add(SlotSlug, SlotData);
	}

	EquipVis->RefreshAllSlots(StateData);

	// Pass the same equipment state to the cosmetic component so that
	// visibility rules are evaluated (e.g. helmet hides hair).
	if (UCosmeticVisualComponent* CosmeticVis = Actor->GetCosmeticVisualComponent())
	{
		CosmeticVis->ApplyEquipmentState(StateData);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Login-level nameplate canvas
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::EnsureLoginNameplateCanvas()
{
	if (IsValid(LoginNameplateCanvas) || !GameInstanceRef) return;
	if (!GameInstanceRef->LoginNameplateCanvasClass) return;

	UWorld* World = GameInstanceRef->GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, GameInstanceRef->LoginNameplateCanvasClass);
	LoginNameplateCanvas = Cast<UNameplateCanvasWidget>(Widget);
	if (LoginNameplateCanvas)
	{
		// Z-order 1 keeps nameplates behind the LoginFlowWidget (Z=10).
		LoginNameplateCanvas->AddToViewport(1);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Click-to-select
// ─────────────────────────────────────────────────────────────────────────────

int32 UCharacterPreviewManager::GetPreviewActorIndex(const ABasicPlayer* Actor) const
{
	for (int32 i = 0; i < SelectPreviewActors.Num(); ++i)
	{
		if (SelectPreviewActors[i] == Actor) return i;
	}
	return -1;
}
