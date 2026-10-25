// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Prototyping.h"
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

void ADroppedItemActor::SetupItemVisuals_Implementation()
{
	if (!ItemMesh)
	{
		UE_LOG(LogGameplay, Error, TEXT("ItemMesh component is null in DroppedItemActor"));
		return;
	}

	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	if (!GameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("Failed to get GameInstance in DroppedItemActor"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("Item manager not found in DroppedItemActor"));
		return;
	}

	UE_LOG(LogGameplay, Verbose, TEXT("Setting up visuals for item %s (Slug: %s)"),
		*ItemData.item.name, *ItemData.item.slug);

	// пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
	VisualData = ItemManager->GetItemVisualDataBySlug(ItemData.item.slug);

	// ---------- Mesh ----------
	bool bAppliedMesh = false;
	{
		const FSoftObjectPath MeshPath = VisualData.ItemMesh.ToSoftObjectPath();
		UE_LOG(LogGameplay, Verbose, TEXT("Loot Mesh path: %s"), *MeshPath.ToString());

		if (MeshPath.IsValid())
		{
			if (UStaticMesh* Mesh = VisualData.ItemMesh.LoadSynchronous())
			{
				ItemMesh->SetStaticMesh(Mesh);
				ItemMesh->SetRelativeScale3D(VisualData.MeshScale);
				bAppliedMesh = true;
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("Failed to LoadSynchronous mesh: %s"), *MeshPath.ToString());
			}
		}

		if (!bAppliedMesh)
		{
			// Fallback пїЅпїЅ пїЅпїЅпїЅпїЅ
			FString FallbackPath;
			switch (ItemData.item.itemType)
			{
			case EItemType::Weapon:     FallbackPath = TEXT("/Engine/BasicShapes/Cube.Cube");      break;
			case EItemType::Armor:      FallbackPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break;
			case EItemType::Consumable: FallbackPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");  break;
			case EItemType::Resource:   FallbackPath = TEXT("/Engine/BasicShapes/Cone.Cone");      break;
			default:                    FallbackPath = TEXT("/Engine/BasicShapes/Cube.Cube");      break;
			}

			if (UStaticMesh* DefaultMesh =
				Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *FallbackPath)))
			{
				ItemMesh->SetStaticMesh(DefaultMesh);
				ItemMesh->SetRelativeScale3D(FVector(0.5f));
				UE_LOG(LogGameplay, Warning, TEXT("Applied fallback mesh: %s"), *FallbackPath);
			}
		}
	}

	// ---------- Material ----------
	{
		const FSoftObjectPath MatPath = VisualData.CustomMaterial.ToSoftObjectPath();
		UE_LOG(LogGameplay, Verbose, TEXT("Loot Material path: %s"), *MatPath.ToString());

		if (MatPath.IsValid())
		{
			if (UMaterialInterface* Mat = VisualData.CustomMaterial.LoadSynchronous())
			{
				ItemMesh->SetMaterial(0, Mat);
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("Failed to LoadSynchronous material: %s"), *MatPath.ToString());
			}
		}
		else
		{
			// пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
			const int32 Rarity = GetItemRarity();
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ItemMesh->GetMaterial(0), this))
			{
				FLinearColor Color = FLinearColor::White;
				switch (Rarity)
				{
				case 2: Color = FLinearColor::Green; break;
				case 3: Color = FLinearColor::Blue; break;
				case 4: Color = FLinearColor(0.5f, 0.f, 0.5f); break;
				case 5: Color = FLinearColor(1.f, 0.5f, 0.f); break;
				}
				MID->SetVectorParameterValue(TEXT("Color"), Color);
				ItemMesh->SetMaterial(0, MID);
			}
		}
	}

	// ---------- Drop Niagara (idle loop on ground) ----------
	if (DropNiagaraComponent)
	{
		const FSoftObjectPath FXPath = VisualData.DropNiagaraSystem.ToSoftObjectPath();
		UE_LOG(LogGameplay, Verbose, TEXT("Loot DropNiagara path: %s"), *FXPath.ToString());

		if (FXPath.IsValid())
		{
			if (UNiagaraSystem* NS = VisualData.DropNiagaraSystem.LoadSynchronous())
			{
				DropNiagaraComponent->SetAsset(NS);
				DropNiagaraComponent->Activate(true);
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("Failed to LoadSynchronous DropNiagaraSystem: %s"), *FXPath.ToString());
			}
		}
	}

	// ---------- Drop SFX ----------
	{
		const FSoftObjectPath SndPath = VisualData.DropSound.ToSoftObjectPath();
		UE_LOG(LogGameplay, Verbose, TEXT("Loot DropSound path: %s"), *SndPath.ToString());

		if (SndPath.IsValid())
		{
			if (USoundCue* Cue = VisualData.DropSound.LoadSynchronous())
			{
				USoundAttenuation* Atn = VisualData.DefaultAttenuation.LoadSynchronous();
				PlayItemSFX(this, Cue, Atn);
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("Failed to LoadSynchronous sound: %s"), *SndPath.ToString());
			}
		}
	}

	OnVisualsSetup();
	UE_LOG(LogGameplay, Verbose, TEXT("SetupItemVisuals completed for item %s (Slug: %s)"),
		*ItemData.item.name, *ItemData.item.slug);
}


