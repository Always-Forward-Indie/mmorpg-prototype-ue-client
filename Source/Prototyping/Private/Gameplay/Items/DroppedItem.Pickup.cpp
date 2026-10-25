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

bool ADroppedItemActor::AttemptPickup()
{
	if (!ItemData.canBePickedUp)
	{
		UE_LOG(LogGameplay, Warning, TEXT("Item %s (ID:%d) cannot be picked up"), *ItemData.item.name, ItemData.uid);
		return false;
	}

	ItemData.canBePickedUp = false;

	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	if (!GameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("Game instance not found"));
		return false;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("Item manager not found"));
		return false;
	}

	// Send pickup request to the server
	ItemManager->SendPickUpItemRequest(ItemData.uid);

	return true;
}

void ADroppedItemActor::Interact()
{
	// When interacted with, attempt to pick up the item
	AttemptPickup();
}

void ADroppedItemActor::OnVisualsSetup()
{
	bVisualsSetupComplete = true;
}

void ADroppedItemActor::PlayPickupEffect()
{
	// Stop and hide the idle drop effect
	if (DropNiagaraComponent)
	{
		DropNiagaraComponent->Deactivate();
	}

	// Pickup SFX пїЅ routed through SFX SoundClass so the volume slider works
	if (!VisualData.PickupSound.IsNull())
	{
		if (USoundCue* Cue = VisualData.PickupSound.LoadSynchronous())
		{
			USoundAttenuation* Atn = VisualData.DefaultAttenuation.LoadSynchronous();
			PlayItemSFX(this, Cue, Atn);
		}
	}

	// Spawn one-shot pickup Niagara at the item location
	const FSoftObjectPath FXPath = VisualData.PickupNiagaraSystem.ToSoftObjectPath();
	if (FXPath.IsValid())
	{
		if (UNiagaraSystem* NS = VisualData.PickupNiagaraSystem.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				NS,
				GetActorLocation(),
				GetActorRotation(),
				FVector(1.f),
				/*bAutoDestroy=*/true,
				/*bAutoActivate=*/true,
				ENCPoolMethod::AutoRelease
			);
		}
		else
		{
			UE_LOG(LogGameplay, Warning, TEXT("PlayPickupEffect: Failed to load PickupNiagaraSystem: %s"), *FXPath.ToString());
		}
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetLifeSpan(0.5f);
}

