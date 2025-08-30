#include "Gameplay/Items/HarvestManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"

UHarvestManager::UHarvestManager()
{
	// Initialize references
	networkManager = nullptr;
	gameInstance = nullptr;
	worldContext = nullptr;
	HarvestProgressWidget = nullptr;
	HarvestLootWidget = nullptr;

	// Initialize harvest state
	bIsHarvesting = false;
	CurrentCorpseUID = 0;
	HarvestStartTime = 0.0f;
	HarvestDuration = 0.0f;
	ServerStartTime = 0;

	// Initialize settings
	MaxHarvestDistance = 300.0f;
	bAutoShowLootWindow = true;

	// Initialize loot data
	CurrentAvailableLoot.Empty();
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Constructor - UObject version"));
}

void UHarvestManager::BeginPlay()
{
	// Start ticking when harvest manager begins play
	StartTicking();
}

void UHarvestManager::Tick(float DeltaTime)
{
	// Update harvest progress if harvesting
	if (bIsHarvesting)
	{
		// Add debug logging to see if tick is working
		static float LastTickLog = 0.0f;
		UWorld* World = GetValidWorld();
		if (World)
		{
			float CurrentTime = World->GetTimeSeconds();
			if (CurrentTime - LastTickLog > 1.0f) // Log every second
			{
				UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Tick - bIsHarvesting: %s, CurrentTime: %.2f, HarvestStartTime: %.2f, HarvestDuration: %.0f"), 
					bIsHarvesting ? TEXT("true") : TEXT("false"), CurrentTime, HarvestStartTime, HarvestDuration);
				LastTickLog = CurrentTime;
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HarvestManager: Tick - GetValidWorld() returned null!"));
		}
		
		UpdateHarvestProgress(DeltaTime);
	}
}

void UHarvestManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("HarvestManager: GameInstance found"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HarvestManager: GameInstance not found"));
		}
	}

	// Start ticking after initialization
	BeginPlay();
}

void UHarvestManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: World context set to %s"), World ? TEXT("Valid") : TEXT("NULL"));
}

void UHarvestManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UHarvestManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Network Manager found and subscribed to events"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Network Manager is valid"));

			// Subscribe to the network manager's events
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UHarvestManager::ProcessGameServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UHarvestManager::ProcessGameServerData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HarvestManager: Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Network manager not found"));
	}
}

void UHarvestManager::TryHarvestNearbyCorpse()
{
	if (bIsHarvesting)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Already harvesting"));
		return;
	}

	ABasicMOB* NearestCorpse = GetNearestHarvestableCorpse(MaxHarvestDistance);
	if (NearestCorpse)
	{
		int32 CorpseUID = FCString::Atoi(*NearestCorpse->GetMOBUId());
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Starting harvest for corpse UID: %d"), CorpseUID);
		StartHarvest(CorpseUID);
	}
	else
	{
		// Check for already harvested corpses to inspect
		ABasicMOB* NearestHarvestedCorpse = GetNearestHarvestedCorpse(MaxHarvestDistance);
		if (NearestHarvestedCorpse)
		{
			int32 CorpseUID = FCString::Atoi(*NearestHarvestedCorpse->GetMOBUId());
			UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Inspecting harvested corpse UID: %d"), CorpseUID);
			InspectCorpseLoot(CorpseUID);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("HarvestManager: No harvestable or harvested corpse found within range"));
			
			// Show message to player
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
					TEXT("No harvestable or harvested corpse found within range"));
			}
		}
	}
}

void UHarvestManager::StartHarvest(int32 CorpseUID)
{
	if (bIsHarvesting)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Already harvesting"));
		return;
	}

	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot start harvest - missing dependencies"));
		return;
	}

	CurrentCorpseUID = CorpseUID;
	SendHarvestStartRequest(CorpseUID);
}

void UHarvestManager::PickupLootItem(int32 ItemId, int32 Quantity)
{
	if (CurrentCorpseUID == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: No active corpse to pickup from"));
		return;
	}

	TArray<FCorpseLootPickupRequestItem> RequestedItems;
	FCorpseLootPickupRequestItem Item;
	Item.itemId = ItemId;
	Item.quantity = Quantity;
	RequestedItems.Add(Item);

	SendLootPickupRequest(CurrentCorpseUID, RequestedItems);
}

void UHarvestManager::PickupAllLoot()
{
	if (CurrentCorpseUID == 0 || CurrentAvailableLoot.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: No loot available to pickup"));
		return;
	}

	TArray<FCorpseLootPickupRequestItem> RequestedItems;
	for (const FHarvestItemStruct& LootItem : CurrentAvailableLoot)
	{
		FCorpseLootPickupRequestItem Item;
		Item.itemId = LootItem.itemId;
		Item.quantity = LootItem.quantity;
		RequestedItems.Add(Item);
	}

	SendLootPickupRequest(CurrentCorpseUID, RequestedItems);
}

void UHarvestManager::CancelHarvest()
{
	if (!bIsHarvesting)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Harvest cancelled"));

	bIsHarvesting = false;
	CurrentCorpseUID = 0;
	HarvestStartTime = 0.0f;
	HarvestDuration = 0.0f;
	ServerStartTime = 0;

	HideHarvestProgress();
}

float UHarvestManager::GetHarvestProgress() const
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager::GetHarvestProgress - Starting calculation"));
	
	if (!bIsHarvesting || HarvestDuration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager::GetHarvestProgress - Not harvesting or invalid duration. bIsHarvesting: %s, HarvestDuration: %.0f"), 
			bIsHarvesting ? TEXT("true") : TEXT("false"), HarvestDuration);
		return 0.0f;
	}

	UWorld* World = GetValidWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager::GetHarvestProgress - GetValidWorld() returned null!"));
		return 0.0f;
	}

	float CurrentTime = World->GetTimeSeconds();
	float ElapsedTime = CurrentTime - HarvestStartTime;
	float TotalDurationInSeconds = HarvestDuration / 1000.0f;
	float Progress = FMath::Clamp(ElapsedTime / TotalDurationInSeconds, 0.0f, 1.0f);
	
	// Always log for debugging
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager::GetHarvestProgress - Progress: %.1f%%, ElapsedTime: %.2fs, TotalDuration: %.2fs, HarvestDuration(ms): %.0f"), 
		Progress * 100.0f, ElapsedTime, TotalDurationInSeconds, HarvestDuration);
	
	return Progress;
}

ABasicMOB* UHarvestManager::GetNearestHarvestableCorpse(float MaxDistance) const
{
	if (!worldContext)
	{
		return nullptr;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return nullptr;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find all MOB actors in the world
	TArray<AActor*> FoundMobs;
	UGameplayStatics::GetAllActorsOfClass(worldContext, ABasicMOB::StaticClass(), FoundMobs);

	ABasicMOB* ClosestCorpse = nullptr;
	float ClosestDistance = MaxDistance;

	// Find the closest harvestable mob within range
	for (AActor* Actor : FoundMobs)
	{
		ABasicMOB* Mob = Cast<ABasicMOB>(Actor);
		if (Mob && Mob->CanBeHarvested())
		{
			float Distance = FVector::Dist(PlayerLocation, Mob->GetActorLocation());

			if (Distance <= MaxDistance && Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestCorpse = Mob;
			}
		}
	}

	return ClosestCorpse;
}

void UHarvestManager::TryInspectNearbyCorpseLoot()
{
	if (bIsHarvesting)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Cannot inspect while harvesting"));
		return;
	}

	ABasicMOB* NearestHarvestedCorpse = GetNearestHarvestedCorpse(MaxHarvestDistance);
	if (NearestHarvestedCorpse)
	{
		int32 CorpseUID = FCString::Atoi(*NearestHarvestedCorpse->GetMOBUId());
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Inspecting harvested corpse UID: %d"), CorpseUID);
		InspectCorpseLoot(CorpseUID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: No harvested corpse found within range"));
		
		// Show message to player
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
				TEXT("No harvested corpse found within range"));
		}
	}
}

void UHarvestManager::InspectCorpseLoot(int32 CorpseUID)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot inspect corpse loot - missing dependencies"));
		return;
	}

	SendLootInspectRequest(CorpseUID);
}

ABasicMOB* UHarvestManager::GetNearestHarvestedCorpse(float MaxDistance) const
{
	if (!worldContext)
	{
		return nullptr;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return nullptr;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find all MOB actors in the world
	TArray<AActor*> FoundMobs;
	UGameplayStatics::GetAllActorsOfClass(worldContext, ABasicMOB::StaticClass(), FoundMobs);

	ABasicMOB* ClosestHarvestedCorpse = nullptr;
	float ClosestDistance = MaxDistance;

	// Find the closest harvested mob within range
	for (AActor* Actor : FoundMobs)
	{
		ABasicMOB* Mob = Cast<ABasicMOB>(Actor);
		if (Mob && Mob->GetMOBIsDead() && Mob->HasBeenHarvested())
		{
			float Distance = FVector::Dist(PlayerLocation, Mob->GetActorLocation());

			if (Distance <= MaxDistance && Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestHarvestedCorpse = Mob;
			}
		}
	}

	return ClosestHarvestedCorpse;
}

void UHarvestManager::SetHarvestProgressWidget(UHarvestProgressWidget* InProgressWidget)
{
	HarvestProgressWidget = InProgressWidget;
	if (HarvestProgressWidget)
	{
		// Set the harvest manager reference in the widget
		HarvestProgressWidget->SetHarvestManager(this);
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Harvest progress widget set and connected"));
	}
}

void UHarvestManager::SetHarvestLootWidget(UHarvestLootWidget* InLootWidget)
{
	HarvestLootWidget = InLootWidget;
	if (HarvestLootWidget)
	{
		// Set the harvest manager reference in the widget
		HarvestLootWidget->SetHarvestManager(this);
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Harvest loot widget set and connected"));
	}
}

void UHarvestManager::ShowHarvestProgress()
{
	if (HarvestProgressWidget)
	{
		HarvestProgressWidget->ShowWidget();
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Showing harvest progress"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot show harvest progress - widget is null"));
	}
}

void UHarvestManager::HideHarvestProgress()
{
	if (HarvestProgressWidget)
	{
		HarvestProgressWidget->HideWidget();
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Hiding harvest progress"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot hide harvest progress - widget is null"));
	}
}

void UHarvestManager::ShowLootWindow()
{
	if (HarvestLootWidget)
	{
		HarvestLootWidget->SetLootItems(CurrentAvailableLoot);
		HarvestLootWidget->ShowWidget();
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Showing loot window"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot show loot window - widget is null"));
	}
}

void UHarvestManager::HideLootWindow()
{
	if (HarvestLootWidget)
	{
		HarvestLootWidget->HideWidget();
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Hiding loot window"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot hide loot window - widget is null"));
	}
}

void UHarvestManager::ProcessGameServerData(const FString& ReceivedData)
{
	// Process time sync data first
	//if (gameInstance && gameInstance->GetTimeSyncService())
	//{
	//	JSONParser::ProcessTimeSyncFromHeader(ReceivedData, gameInstance->GetTimeSyncService());
	//}

	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received event type: %s"), *MessageData.eventType);

	// Handle harvest started response
	if (MessageData.eventType == "harvestStarted" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received harvest started"));
		FHarvestStartedStruct HarvestData = JSONParser::DeserializeHarvestStarted(ReceivedData);
		
		// Validate harvest data before proceeding
		if (HarvestData.duration <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("HarvestManager: Invalid harvest duration: %d ms"), HarvestData.duration);
			return;
		}
		
		if (HarvestData.corpseId <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("HarvestManager: Invalid corpse ID: %d"), HarvestData.corpseId);
			return;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: About to set harvest state variables"));
		
		// Start local harvest progress
		bIsHarvesting = true;
		CurrentCorpseUID = HarvestData.corpseId;
		HarvestDuration = HarvestData.duration;
		ServerStartTime = HarvestData.startTime;
		
		UWorld* World = GetValidWorld();
		if (World)
		{
			HarvestStartTime = World->GetTimeSeconds();
			UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Successfully got World time: %.2f"), HarvestStartTime);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HarvestManager: GetValidWorld() returned null when setting HarvestStartTime!"));
			HarvestStartTime = 0.0f;
		}

		// Add detailed debug logging
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Harvest started - CorpseID: %d, Duration: %dms (%.2fs), ServerStartTime: %lld, ClientStartTime: %.2fs"), 
			HarvestData.corpseId, HarvestData.duration, HarvestData.duration / 1000.0f, HarvestData.startTime, HarvestStartTime);

		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: State after setting - bIsHarvesting: %s, HarvestDuration: %.0f, HarvestStartTime: %.2f"), 
			bIsHarvesting ? TEXT("true") : TEXT("false"), HarvestDuration, HarvestStartTime);

		ShowHarvestProgress();
		OnHarvestStarted.Broadcast(HarvestData);
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Harvest started event processing completed"));
	}
	// Handle harvest complete response
	else if (MessageData.eventType == "harvestComplete" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received harvest complete"));
		FHarvestCompleteStruct HarvestData = JSONParser::DeserializeHarvestComplete(ReceivedData);
		HandleHarvestComplete(HarvestData);
	}
	// Handle harvest error
	else if (MessageData.eventType == "harvestError" && MessageData.status == "error")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received harvest error"));
		FHarvestErrorStruct ErrorData = JSONParser::DeserializeHarvestError(ReceivedData);
		
		// Cancel local harvest
		CancelHarvest();
		
		// Show error message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
				FString::Printf(TEXT("Harvest Error: %s"), *ErrorData.message));
		}

		OnHarvestError.Broadcast(ErrorData);
	}
	// Handle loot pickup success
	else if (MessageData.eventType == "corpseLootPickup" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received loot pickup success"));
		FCorpseLootPickupResponseStruct PickupData = JSONParser::DeserializeCorpseLootPickupResponse(ReceivedData);
		HandleLootPickupResponse(PickupData);
	}
	// Handle loot pickup error
	else if (MessageData.eventType == "corpseLootPickup" && MessageData.status == "error")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received loot pickup error"));
		FCorpseLootPickupErrorStruct ErrorData = JSONParser::DeserializeCorpseLootPickupError(ReceivedData);
		
		// Show error message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
				FString::Printf(TEXT("Loot Pickup Error: %s"), *ErrorData.errorCode));
		}

		OnLootPickupError.Broadcast(ErrorData);
	}
	// Handle loot inspect success
	else if (MessageData.eventType == "corpseLootInspect" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received loot inspect success"));
		FCorpseLootInspectResponseStruct InspectData = JSONParser::DeserializeCorpseLootInspectResponse(ReceivedData);
		HandleLootInspectResponse(InspectData);
	}
	// Handle loot inspect error
	else if (MessageData.eventType == "corpseLootInspect" && MessageData.status == "error")
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Received loot inspect error"));
		FCorpseLootInspectErrorStruct ErrorData = JSONParser::DeserializeCorpseLootInspectError(ReceivedData);
		
		// Show error message based on error code
		FString ErrorMessage = TEXT("Loot Inspect Error");
		if (ErrorData.errorCode == "CORPSE_NOT_FOUND")
		{
			ErrorMessage = TEXT("Corpse not found");
		}
		else if (ErrorData.errorCode == "CORPSE_NOT_HARVESTED")
		{
			ErrorMessage = TEXT("Corpse has not been harvested yet");
		}
		else if (ErrorData.errorCode == "NOT_YOUR_HARVEST")
		{
			ErrorMessage = TEXT("You can only inspect loot from corpses you harvested");
		}
		else if (ErrorData.errorCode == "SECURITY_VIOLATION")
		{
			ErrorMessage = TEXT("Security violation: player ID mismatch");
		}
		else
		{
			ErrorMessage = FString::Printf(TEXT("Loot Inspect Error: %s"), *ErrorData.errorCode);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, ErrorMessage);
		}

		OnLootInspectError.Broadcast(ErrorData);
	}
}

void UHarvestManager::SendHarvestStartRequest(int32 CorpseUID)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot send harvest start request - missing dependencies"));
		return;
	}

	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	
	// Add client ID and hash from the game instance
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID()));
	HeaderData.Add(TEXT("clientId"), ClientIDValue);
	
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash()));
	HeaderData.Add(TEXT("hash"), HashValue);

	TSharedPtr<FJsonValueString> MessageValue = MakeShareable(new FJsonValueString(TEXT("start harvest")));
	HeaderData.Add(TEXT("message"), MessageValue);
	
	// Add the corpse UID to the body data
	TSharedPtr<FJsonValueNumber> CorpseUIDValue = MakeShareable(new FJsonValueNumber(CorpseUID));
	BodyData.Add(TEXT("corpseUID"), CorpseUIDValue);
	
	// Create the JSON string with automatic clientSendMs and correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("harvestStart"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Sent harvest start request for corpse: %d"), CorpseUID);
}

void UHarvestManager::SendLootPickupRequest(int32 CorpseUID, const TArray<FCorpseLootPickupRequestItem>& RequestedItems)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot send loot pickup request - missing dependencies"));
		return;
	}

	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	
	// Add client ID and hash from the game instance
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID()));
	HeaderData.Add(TEXT("clientId"), ClientIDValue);
	
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash()));
	HeaderData.Add(TEXT("hash"), HashValue);

	TSharedPtr<FJsonValueString> MessageValue = MakeShareable(new FJsonValueString(TEXT("pickup corpse loot")));
	HeaderData.Add(TEXT("message"), MessageValue);
	
	// Add player ID and corpse UID to the body data
	TSharedPtr<FJsonValueNumber> PlayerIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID()));
	BodyData.Add(TEXT("playerId"), PlayerIDValue);

	TSharedPtr<FJsonValueNumber> CorpseUIDValue = MakeShareable(new FJsonValueNumber(CorpseUID));
	BodyData.Add(TEXT("corpseUID"), CorpseUIDValue);

	// Create requested items array
	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const FCorpseLootPickupRequestItem& Item : RequestedItems)
	{
		TSharedPtr<FJsonObject> ItemObj = MakeShareable(new FJsonObject);
		ItemObj->SetNumberField(TEXT("itemId"), Item.itemId);
		ItemObj->SetNumberField(TEXT("quantity"), Item.quantity);
		ItemsArray.Add(MakeShareable(new FJsonValueObject(ItemObj)));
	}

	TSharedPtr<FJsonValueArray> RequestedItemsArray = MakeShareable(new FJsonValueArray(ItemsArray));
	BodyData.Add(TEXT("requestedItems"), RequestedItemsArray);
	
	// Create the JSON string with automatic clientSendMs and correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("corpseLootPickup"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Sent loot pickup request for corpse: %d with %d items"), 
		CorpseUID, RequestedItems.Num());
}

void UHarvestManager::SendLootInspectRequest(int32 CorpseUID)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot send loot inspect request - missing dependencies"));
		return;
	}

	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	
	// Add client ID and hash from the game instance
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID()));
	HeaderData.Add(TEXT("clientId"), ClientIDValue);
	
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash()));
	HeaderData.Add(TEXT("hash"), HashValue);

	TSharedPtr<FJsonValueString> MessageValue = MakeShareable(new FJsonValueString(TEXT("inspect corpse loot")));
	HeaderData.Add(TEXT("message"), MessageValue);
	
	// Add player ID and corpse UID to the body data
	TSharedPtr<FJsonValueNumber> PlayerIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID()));
	BodyData.Add(TEXT("playerId"), PlayerIDValue);

	TSharedPtr<FJsonValueNumber> CorpseUIDValue = MakeShareable(new FJsonValueNumber(CorpseUID));
	BodyData.Add(TEXT("corpseUID"), CorpseUIDValue);
	
	// Create the JSON string with automatic clientSendMs and correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("corpseLootInspect"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Sent loot inspect request for corpse: %d"), CorpseUID);
}

void UHarvestManager::UpdateHarvestProgress(float DeltaTime)
{
	if (!bIsHarvesting)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager::UpdateHarvestProgress - Not harvesting, returning early"));
		return;
	}

	// Calculate current progress
	float Progress = GetHarvestProgress();
	
	// Always broadcast progress updates (let the widget decide if it needs to update UI)
	OnHarvestProgressUpdate.Broadcast(Progress);
	
	// Log progress changes more frequently for debugging
	static float LastLoggedProgress = -1.0f;
	if (FMath::Abs(Progress - LastLoggedProgress) > 0.05f) // Log every 5% change
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager::UpdateHarvestProgress - Broadcasting progress %.1f%%"), 
			Progress * 100.0f);
		LastLoggedProgress = Progress;
	}

	// Auto-complete if progress reaches 100% (fallback in case server message is missed)
	if (Progress >= 1.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Harvest progress completed locally"));
		// Note: We don't auto-complete here, we wait for server response
	}
}

void UHarvestManager::HandleHarvestComplete(const FHarvestCompleteStruct& HarvestData)
{
	// End harvest state
	bIsHarvesting = false;
	HideHarvestProgress();

	// Store available loot
	CurrentAvailableLoot = HarvestData.availableLoot;
	CurrentCorpseUID = HarvestData.corpseId;

	// Mark the mob as harvested
	if (worldContext)
	{
		FString CorpseUIDStr = FString::FromInt(HarvestData.corpseId);
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(worldContext, FName(*CorpseUIDStr), FoundActors);

		if (FoundActors.Num() > 0)
		{
			ABasicMOB* Mob = Cast<ABasicMOB>(FoundActors[0]);
			if (Mob)
			{
				Mob->SetHarvested(true);
				UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Marked mob %s as harvested"), *Mob->GetMobName());
			}
		}
	}

	// Show loot window if enabled and there's loot
	if (bAutoShowLootWindow && CurrentAvailableLoot.Num() > 0)
	{
		ShowLootWindow();
	}

	// Show completion message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
			FString::Printf(TEXT("Harvest completed! Found %d items."), HarvestData.totalItems));
	}

	OnHarvestCompleted.Broadcast(HarvestData);
}

void UHarvestManager::HandleLootPickupResponse(const FCorpseLootPickupResponseStruct& PickupData)
{
	// Update available loot with remaining items
	CurrentAvailableLoot = PickupData.remainingLoot;

	// Add picked up items to inventory
	if (gameInstance)
	{
		UInventoryManager* InventoryManager = gameInstance->GetInventoryManager();
		if (InventoryManager)
		{
			for (const FHarvestItemStruct& PickedUpItem : PickupData.pickedUpItems)
			{
				// Convert HarvestItemStruct to InventoryItemStruct
				FInventoryItemStruct InventoryItem;
				InventoryItem.itemId = PickedUpItem.itemId;
				InventoryItem.quantity = PickedUpItem.quantity;
				InventoryItem.name = PickedUpItem.name;
				InventoryItem.slug = PickedUpItem.itemSlug;
				InventoryItem.description = PickedUpItem.description;
				InventoryItem.type = PickedUpItem.itemType;
				InventoryItem.rarity = PickedUpItem.rarityName;
				InventoryItem.weight = PickedUpItem.weight;
				
				// Add to local inventory (this will trigger UI updates)
				InventoryManager->AddItemToLocalInventory(InventoryItem);
			}
		}
	}

	// Hide loot window if no more loot
	if (CurrentAvailableLoot.Num() == 0)
	{
		HideLootWindow();
		CurrentCorpseUID = 0;
	}

	// Show pickup success message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
			FString::Printf(TEXT("Picked up %d items"), PickupData.itemsPickedUp));
	}

	OnLootPickupSuccess.Broadcast(PickupData);
}

void UHarvestManager::HandleLootInspectResponse(const FCorpseLootInspectResponseStruct& InspectData)
{
	// Store available loot and corpse ID
	CurrentAvailableLoot = InspectData.availableLoot;
	CurrentCorpseUID = InspectData.corpseUID;

	// Show loot window if there's loot available
	if (InspectData.availableLoot.Num() > 0)
	{
		ShowLootWindow();
		
		// Show success message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
				FString::Printf(TEXT("Found %d items in corpse"), InspectData.totalItems));
		}
	}
	else
	{
		// Show message that corpse has no loot
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
				TEXT("No loot remaining in this corpse"));
		}
	}

	OnLootInspectSuccess.Broadcast(InspectData);
}

UWorld* UHarvestManager::GetValidWorld() const
{
	// Use stored world context since we're now a UObject
	return worldContext;
}

void UHarvestManager::StartTicking()
{
	if (GetValidWorld())
	{
		// Start a timer that calls Tick every frame (approximately 60 FPS)
		GetValidWorld()->GetTimerManager().SetTimer(TickTimerHandle, [this]()
		{
			Tick(0.016f); // Approximate delta time for 60 FPS
		}, 0.016f, true);
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Started ticking with timer"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot start ticking - no world context"));
	}
}

void UHarvestManager::StopTicking()
{
	if (GetValidWorld())
	{
		GetValidWorld()->GetTimerManager().ClearTimer(TickTimerHandle);
		UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Stopped ticking"));
	}
}