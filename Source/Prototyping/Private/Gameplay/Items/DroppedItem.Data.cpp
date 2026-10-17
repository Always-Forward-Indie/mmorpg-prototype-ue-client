// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ShapeComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/DroppedItemSFX.h"

void ADroppedItemActor::SetItemData(const FDroppedItemStruct& InItemData)
{
	ItemData = InItemData;

	// Visuals and trajectory are initialised in BeginPlay once the actor is
	// fully spawned. SetItemData is called via SpawnActorDeferred BEFORE
	// BeginPlay, so we must not start the animation here пїЅ BeginPlay will do it.
	// If SetItemData is called AFTER BeginPlay (live update), handle it then.
	if (HasActorBegunPlay())
	{
		if (!bVisualsSetupComplete)
		{
			SetupItemVisuals_Implementation();
		}

		if (!ItemData.droppedByMobUID.IsEmpty())
		{
			TryStartTrajectoryFromMob();
		}
	}
}

int32 ADroppedItemActor::GetItemRarity() const
{
	// Look for a "rarity" attribute
	for (const FItemAttributeStruct& Attribute : ItemData.item.attributes)
	{
		if (Attribute.slug == "rarity" || Attribute.name.ToLower() == "rarity")
		{
			return FMath::Max(1, FMath::Min(5, static_cast<int32>(Attribute.value)));
		}
	}

	// Default to common (1) if no rarity found
	return 1;
}

