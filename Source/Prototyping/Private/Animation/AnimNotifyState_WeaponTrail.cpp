#include "Animation/AnimNotifyState_WeaponTrail.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/ItemStruct.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "MyGameInstance.h"

UAnimNotifyState_WeaponTrail::UAnimNotifyState_WeaponTrail()
{
}

void UAnimNotifyState_WeaponTrail::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// Find EquipmentVisualComponent on the owner
	UEquipmentVisualComponent* EquipVis = Owner->FindComponentByClass<UEquipmentVisualComponent>();
	if (!EquipVis) return;

	// Get the weapon mesh component for the slot
	USceneComponent* WeaponComp = EquipVis->GetSlotComponent(WeaponSlotSlug);
	if (!WeaponComp) return;

	// Get item slug for this slot
	FString ItemSlug = EquipVis->GetItemSlugForSlot(WeaponSlotSlug);
	if (ItemSlug.IsEmpty()) return;

	// Resolve ItemManager from GameInstance
	UMyGameInstance* MyGI = Cast<UMyGameInstance>(Owner->GetGameInstance());
	if (!MyGI) return;

	UItemManager* ItemMgr = MyGI->GetItemManager();
	if (!ItemMgr) return;

	// Look up item visual data
	FItemVisualData VisualData = ItemMgr->GetItemVisualDataBySlug(ItemSlug);

	// Check if swing VFX is defined
	if (VisualData.EquippedSwingVFX.IsNull()) return;

	UNiagaraSystem* SwingVFX = VisualData.EquippedSwingVFX.LoadSynchronous();
	if (!SwingVFX) return;

	// Spawn the Niagara trail attached to the weapon socket
	UNiagaraComponent* NC = NewObject<UNiagaraComponent>(Owner, TEXT("WeaponTrailVFX"));
	NC->SetAsset(SwingVFX);
	NC->RegisterComponent();

	// Attach to the character mesh at the weapon socket so it follows the weapon
	NC->AttachToComponent(MeshComp,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		VisualData.EquipSocketName);
	NC->Activate(true);

	ActiveTrailComponent = NC;

	UE_LOG(LogTemp, Log, TEXT("AnimNotifyState_WeaponTrail: Started swing trail for '%s' on '%s'"),
		*ItemSlug, *Owner->GetName());
}

void UAnimNotifyState_WeaponTrail::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (ActiveTrailComponent.IsValid())
	{
		ActiveTrailComponent->Deactivate();
		ActiveTrailComponent->DestroyComponent();
		ActiveTrailComponent.Reset();
	}
}
