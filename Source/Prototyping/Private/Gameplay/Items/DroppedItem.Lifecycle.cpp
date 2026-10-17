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

DEFINE_LOG_CATEGORY(LogDropSnap);

// Sets default values
ADroppedItemActor::ADroppedItemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create and setup root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Create and attach mesh component
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootSceneComponent);
	ItemMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ItemMesh->SetGenerateOverlapEvents(true);
	ItemMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));

	// Create and attach interaction sphere
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootSceneComponent);
	InteractionSphere->SetSphereRadius(PickupRadius);
	// Block ECC_Visibility so the cursor hover trace can detect this dropped item.
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	// Create and attach Niagara drop effect component (idle loop while on ground)
	DropNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DropNiagaraComponent"));
	DropNiagaraComponent->SetupAttachment(RootSceneComponent);
	DropNiagaraComponent->SetAutoActivate(false);

	// Cursor target-indicator decal (floor circle).
	TargetDecal = CreateDefaultSubobject<UTargetDecalComponent>(TEXT("TargetDecal"));
	TargetDecal->SetupAttachment(RootSceneComponent);

	bVisualsSetupComplete = false;
}

// ─── IWorldInteractable interface ────────────────────────────────────────────
EInteractableType ADroppedItemActor::GetInteractableType() const
{
    return EInteractableType::DroppedItem;
}

FText ADroppedItemActor::GetInteractableDisplayName() const
{
    const int32 Rarity = GetItemRarity();
    if (Rarity > 0)
        return FText::FromString(FString::Printf(TEXT("%s  (%d)"), *GetItemName(), Rarity));
    return FText::FromString(GetItemName());
}

bool ADroppedItemActor::CanInteract() const
{
    return ItemData.canBePickedUp;
}

// Called when the game starts or when spawned
