// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationToastWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"
#include "CrashDiagnostics.h"

void UItemManager::ProcessItemPickup(const FItemBaseStruct& Item)
{
	// Broadcast the item pickup event
	OnItemPickedUp.Broadcast(Item);

	UE_LOG(LogTemp, Warning, TEXT("Picked up item: %s"), *Item.name);

	// Show a toast notification with localised item name
	if (!Item.slug.IsEmpty())
	{
		if (ABasicPlayer* Player = Cast<ABasicPlayer>(GetOwner()))
		{
			if (UUIManager* UIMgr = Player->GetUIManager())
			{
				if (UWorldNotificationManager* NotifMgr = UIMgr->GetWorldNotificationManager())
				{
					if (UNotificationToastWidget* Toast = NotifMgr->GetToastWidget())
					{
						TMap<FString, FString> DataFields;
						DataFields.Add(TEXT("itemSlug"), Item.slug);
						DataFields.Add(TEXT("itemId"), FString::FromInt(Item.id));
						DataFields.Add(TEXT("quantity"), TEXT("1"));
						Toast->EnqueueActionToast(TEXT("item_received"), DataFields);
					}
				}
			}
		}
	}
}

// Called when the game starts
void UItemManager::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void UItemManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UItemManager::LoadItemVisualsDataTable(UDataTable* InItemVisualsTable)
{
	ItemVisualsDataTable = InItemVisualsTable;

	// Clear existing cache
	ItemVisualsCache.Empty();

	// If we have a valid data table, populate the cache
	if (ItemVisualsDataTable)
	{
		TArray<FName> RowNames = ItemVisualsDataTable->GetRowNames();

		for (const FName& RowName : RowNames)
		{
			FItemVisualData* VisualData = ItemVisualsDataTable->FindRow<FItemVisualData>(RowName, TEXT("Loading item visuals"));

			if (VisualData)
			{
				ItemVisualsCache.Add(RowName.ToString(), *VisualData);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Loaded %d item visual entries from data table"), ItemVisualsCache.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No item visuals data table provided"));
	}

	PreloadNiagaraAssetsAsync();
}

void UItemManager::PreloadNiagaraAssetsAsync()
{
	TArray<FSoftObjectPath> Paths;
	Paths.Reserve(ItemVisualsCache.Num() * 4);

	for (const auto& Pair : ItemVisualsCache)
	{
		const FItemVisualData& V = Pair.Value;
		if (!V.DropNiagaraSystem.IsNull())    Paths.Add(V.DropNiagaraSystem.ToSoftObjectPath());
		if (!V.PickupNiagaraSystem.IsNull())  Paths.Add(V.PickupNiagaraSystem.ToSoftObjectPath());
		if (!V.EquippedSwingVFX.IsNull())     Paths.Add(V.EquippedSwingVFX.ToSoftObjectPath());
		if (!V.EquippedIdleVFX.IsNull())      Paths.Add(V.EquippedIdleVFX.ToSoftObjectPath());
	}

	if (Paths.Num() == 0) return;

	FStreamableManager& SM = UAssetManager::GetStreamableManager();
	NiagaraPreloadHandle = SM.RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateLambda([](){}),
		FStreamableManager::AsyncLoadHighPriority
	);

	UE_LOG(LogTemp, Log, TEXT("ItemManager: Preloading %d Niagara VFX assets"), Paths.Num());
}

FItemVisualData UItemManager::GetItemVisualDataBySlug(const FString& ItemSlug)
{
	if (ItemSlug.IsEmpty()) return FItemVisualData();

	// Check cache first
	if (ItemVisualsCache.Contains(ItemSlug))
	{
		return ItemVisualsCache[ItemSlug];
	}

	if (ItemVisualsDataTable)
	{
		FItemVisualData* VisualData = ItemVisualsDataTable->FindRow<FItemVisualData>(FName(*ItemSlug), TEXT("GetItemVisualDataBySlug"), false);
		if (VisualData)
		{
			ItemVisualsCache.Add(ItemSlug, *VisualData);
			return *VisualData;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ItemManager: No visual data found for slug: %s"), *ItemSlug);
	return FItemVisualData();
}

void UItemManager::SetItemCatalog(int32 Version, const TArray<FInventoryItemStruct>& Items)
{
	// New version replaces the cache; same-version chunks accumulate
	// (the server splits the catalog across 8KB-capped lines).
	if (Version != ItemCatalogVersion)
	{
		ItemStatCatalog.Empty();
		ItemCatalogVersion = Version;
	}
	for (const FInventoryItemStruct& Item : Items)
	{
		if (Item.itemId > 0)
			ItemStatCatalog.Add(Item.itemId, Item);
	}
	UE_LOG(LogTemp, Log, TEXT("ItemManager: Stat catalog v%d now holds %d templates (+%d)"),
		ItemCatalogVersion, ItemStatCatalog.Num(), Items.Num());

	// New statics may complete placeholder rows the inventory merged from
	// slim deltas before the catalog arrived (join race).
	if (gameInstance)
	{
		if (UInventoryManager* InvMgr = gameInstance->GetInventoryManager())
			InvMgr->ResolveUnresolvedRows();
	}
}

bool UItemManager::GetCatalogStats(int32 ItemId, FInventoryItemStruct& OutStats) const
{
	if (ItemId <= 0) return false;
	if (const FInventoryItemStruct* Found = ItemStatCatalog.Find(ItemId))
	{
		OutStats = *Found;
		return true;
	}
	return false;
}

void UItemManager::ResolveDropStats(FDroppedItemStruct& Drop) const
{
	if (!Drop.item.slug.IsEmpty() || Drop.itemId <= 0) return;
	FInventoryItemStruct Stats;
	if (!GetCatalogStats(Drop.itemId, Stats)) return;
	Drop.item.id = Drop.itemId;
	Drop.item.slug = Stats.slug;
	Drop.item.itemType = static_cast<EItemType>(Stats.item_type_id);
	Drop.item.itemTypeSlug = Stats.itemTypeSlug;
	Drop.item.attributes = Stats.itemAttributes;
	// name/description stay empty: every display path resolves via Loc/humanized slug.
}

// ---------------------------------------------------------------------------
// BindPickupPointDelegate
//   Lazily binds to the local player's PlayerAnimInstance::OnPickupPoint so
//   OnPickupPointFired() is called at the exact animation frame.
//   Safe to call multiple times � binds only once per session.
// ---------------------------------------------------------------------------
