#include "Gameplay/NPCs/NPCManager.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

UNPCManager::UNPCManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	worldContext = nullptr;
	networkManager = nullptr;
	gameInstance = nullptr;
	bIsInitialized = false;
	DefaultSpawnHeight = 200.0f;
	bAutoCleanupInvalidNPCs = true;
	CleanupInterval = 30.0f;
}

void UNPCManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;
	
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());
		
		if (gameInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("NPCManager: GameInstance found and initialized"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("NPCManager: GameInstance not found"));
		}
	}

	bIsInitialized = true;

	// Start cleanup timer if enabled
	if (bAutoCleanupInvalidNPCs && worldContext)
	{
		worldContext->GetTimerManager().SetTimer(
			CleanupTimerHandle,
			this,
			&UNPCManager::CleanupInvalidNPCs,
			CleanupInterval,
			true
		);
	}

	UE_LOG(LogTemp, Warning, TEXT("NPCManager: Initialized successfully"));
}

void UNPCManager::SetWorldContext(UWorld* World)
{
	const bool bWasNull = (worldContext == nullptr);
	worldContext = World;
	UE_LOG(LogTemp, Warning, TEXT("NPCManager: World context set to %s"), World ? TEXT("Valid") : TEXT("NULL"));

	if (bWasNull && worldContext)
	{
		FlushPendingSpawns();
	}

	if (worldContext && bAutoCleanupInvalidNPCs)
	{
		CleanupTimerHandle.Invalidate();
		worldContext->GetTimerManager().SetTimer(
			CleanupTimerHandle,
			this,
			&UNPCManager::CleanupInvalidNPCs,
			CleanupInterval,
			true
		);
	}
}

void UNPCManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UNPCManager::SubscribeToNetworkManager()
{
	if (!networkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Network manager not found"));
		return;
	}

	if (!IsValid(networkManager))
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Network Manager is not valid"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("NPCManager: Network Manager found - NPCNetworkHandler will handle network events"));

	// NPCManager ������ �� ������������� �� ������� ������� ��������
	// ��� ������� ������� �������������� ����� NPCNetworkHandler
	// ������� ����� �������� ������ NPCManager
}

void UNPCManager::ProcessGameServerData(const FString& ReceivedData)
{
	// ���� ����� ������ �� ������������
	// ��� ��������� ������� ������ ���������� � NPCNetworkHandler
	UE_LOG(LogTemp, Warning, TEXT("NPCManager::ProcessGameServerData called - this should not happen! Use NPCNetworkHandler instead."));
}

void UNPCManager::SpawnNPC(const FNPCStruct& NPCData)
{
	if (!ValidateNPCData(NPCData))
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Invalid NPC data provided for NPC ID: %d"), NPCData.id);
		return;
	}

	if (!worldContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: No world context — queuing NPC %d (%s) for later spawn"), NPCData.id, *NPCData.name);
		PendingNPCSpawns.Add(NPCData);
		return;
	}

	if (SpawnedNPCs.Contains(NPCData.id))
	{
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: NPC with ID %d already exists"), NPCData.id);
		return;
	}

	ABasicNPC* SpawnedNPC = CreateNPCActor(NPCData);
	if (SpawnedNPC)
	{
		SpawnedNPCs.Add(NPCData.id, SpawnedNPC);
		
		SpawnedNPC->SetNPCData(NPCData);
		
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: Successfully spawned NPC %s (ID: %d)"), *NPCData.name, NPCData.id);
		
		OnNPCSpawned.Broadcast(SpawnedNPC);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Failed to spawn NPC %s (ID: %d)"), *NPCData.name, NPCData.id);
	}
}

void UNPCManager::SpawnNPCs(const TArray<FNPCStruct>& NPCsData)
{
	for (const FNPCStruct& NPCData : NPCsData)
	{
		SpawnNPC(NPCData);
	}
}

void UNPCManager::RemoveNPC(int32 NPCId)
{
	ABasicNPC** FoundNPC = SpawnedNPCs.Find(NPCId);
	if (FoundNPC && *FoundNPC)
	{
		ABasicNPC* NPCToRemove = *FoundNPC;
		FString NPCName = NPCToRemove->GetNPCName();
		
		// Remove from world
		NPCToRemove->Destroy();
		
		// Remove from map
		SpawnedNPCs.Remove(NPCId);
		
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: Removed NPC %s (ID: %d)"), *NPCName, NPCId);
		
		// Broadcast event
		OnNPCRemoved.Broadcast(NPCId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: NPC with ID %d not found for removal"), NPCId);
	}
}

void UNPCManager::RemoveAllNPCs()
{
	TArray<int32> NPCIds;
	SpawnedNPCs.GetKeys(NPCIds);
	
	for (int32 NPCId : NPCIds)
	{
		RemoveNPC(NPCId);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("NPCManager: Removed all NPCs"));
}

ABasicNPC* UNPCManager::GetNPCById(int32 NPCId) const
{
	ABasicNPC* const* FoundNPC = SpawnedNPCs.Find(NPCId);
	return FoundNPC ? *FoundNPC : nullptr;
}

TArray<ABasicNPC*> UNPCManager::GetAllNPCs() const
{
	TArray<ABasicNPC*> AllNPCs;
	SpawnedNPCs.GenerateValueArray(AllNPCs);
	
	// Filter out null/invalid NPCs
	AllNPCs.RemoveAll([](ABasicNPC* NPC) { return !IsValid(NPC); });
	
	return AllNPCs;
}

TArray<ABasicNPC*> UNPCManager::GetNPCsByType(const FString& NPCType) const
{
	TArray<ABasicNPC*> FilteredNPCs;
	
	for (const auto& NPCPair : SpawnedNPCs)
	{
		if (IsValid(NPCPair.Value) && NPCPair.Value->GetNPCType() == NPCType)
		{
			FilteredNPCs.Add(NPCPair.Value);
		}
	}
	
	return FilteredNPCs;
}

TArray<ABasicNPC*> UNPCManager::GetInteractableNPCs() const
{
	TArray<ABasicNPC*> InteractableNPCs;
	
	for (const auto& NPCPair : SpawnedNPCs)
	{
		if (IsValid(NPCPair.Value) && NPCPair.Value->IsNPCInteractable())
		{
			InteractableNPCs.Add(NPCPair.Value);
		}
	}
	
	return InteractableNPCs;
}

bool UNPCManager::NPCExists(int32 NPCId) const
{
	ABasicNPC* const* FoundNPC = SpawnedNPCs.Find(NPCId);
	return FoundNPC && IsValid(*FoundNPC);
}

ABasicNPC* UNPCManager::GetNearestNPC(const FVector& Location, float MaxDistance) const
{
	ABasicNPC* NearestNPC = nullptr;
	float NearestDistance = MaxDistance;
	
	for (const auto& NPCPair : SpawnedNPCs)
	{
		if (IsValid(NPCPair.Value))
		{
			float Distance = FVector::Dist(Location, NPCPair.Value->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestNPC = NPCPair.Value;
			}
		}
	}
	
	return NearestNPC;
}

TArray<ABasicNPC*> UNPCManager::GetNPCsInRadius(const FVector& Location, float Radius) const
{
	TArray<ABasicNPC*> NPCsInRadius;
	
	for (const auto& NPCPair : SpawnedNPCs)
	{
		if (IsValid(NPCPair.Value))
		{
			float Distance = FVector::Dist(Location, NPCPair.Value->GetActorLocation());
			if (Distance <= Radius)
			{
				NPCsInRadius.Add(NPCPair.Value);
			}
		}
	}
	
	return NPCsInRadius;
}

void UNPCManager::RequestNPCData(const FClientDataStruct& ClientData)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Cannot request NPC data - missing dependencies"));
		return;
	}

	// Create JSON request for NPC data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add client ID and hash to header
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Create JSON string
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("requestNPCData", HeaderData, BodyData, 
		gameInstance->GetTimeSyncService(), EServerType::ChunkServer);

	// Send to chunk server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("NPCManager: Sent NPC data request"));
}

ABasicNPC* UNPCManager::CreateNPCActor(const FNPCStruct& NPCData)
{
	if (!worldContext)
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Cannot spawn NPC - no world context"));
		return nullptr;
	}

	// Get NPC class to spawn
	TSubclassOf<ABasicNPC> NPCClass = DefaultNPCClass;
	if (!NPCClass && gameInstance)
	{
		// Try to get from game instance if available
		NPCClass = gameInstance->GetBasicNPCClass();
	}

	// Fall back to default BasicNPC class
	if (!NPCClass)
	{
		NPCClass = ABasicNPC::StaticClass();
	}

	// Calculate spawn location
	FVector SpawnLocation(NPCData.position.positionX, NPCData.position.positionY, NPCData.position.positionZ);
	FRotator SpawnRotation(0.0f, NPCData.position.rotationZ, 0.0f);

	// Spawn the NPC
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABasicNPC* SpawnedNPC = worldContext->SpawnActor<ABasicNPC>(NPCClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (SpawnedNPC)
	{
		// Set NPC Definition Table if available
		if (NPCDefinitionTable)
		{
			SpawnedNPC->NPCDefinitionTable = NPCDefinitionTable;
			UE_LOG(LogTemp, Log, TEXT("NPCManager: Assigned NPCDefinitionTable to NPC %s"), *NPCData.name);
		}
		else
		{
			// Try to get NPCDefinitionTable from game instance if not set locally
			if (gameInstance && gameInstance->GetNPCManager() && gameInstance->GetNPCManager()->GetNPCDefinitionTable())
			{
				SpawnedNPC->NPCDefinitionTable = gameInstance->GetNPCManager()->GetNPCDefinitionTable();
				UE_LOG(LogTemp, Log, TEXT("NPCManager: Assigned NPCDefinitionTable from GameInstance to NPC %s"), *NPCData.name);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("NPCManager: NPCDefinitionTable not available anywhere, NPC %s will not have visual/audio setup"), *NPCData.name);
			}
		}

		// Set a tag for easy identification
		SpawnedNPC->Tags.Add(FName(*FString::Printf(TEXT("NPC_%d"), NPCData.id)));
		UE_LOG(LogTemp, Log, TEXT("NPCManager: Created NPC actor for %s at location %s"), 
			*NPCData.name, *SpawnLocation.ToString());
	}

	return SpawnedNPC;
}

bool UNPCManager::ValidateNPCData(const FNPCStruct& NPCData) const
{
	if (NPCData.id <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Invalid NPC ID: %d"), NPCData.id);
		return false;
	}

	if (NPCData.name.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("NPCManager: Empty NPC name for ID: %d"), NPCData.id);
		return false;
	}

	return true;
}

void UNPCManager::FlushPendingSpawns()
{
	if (PendingNPCSpawns.Num() == 0) return;

	UE_LOG(LogTemp, Warning, TEXT("NPCManager: Flushing %d pending NPC spawns"), PendingNPCSpawns.Num());
	TArray<FNPCStruct> Spawns = MoveTemp(PendingNPCSpawns);
	PendingNPCSpawns.Empty();

	for (const FNPCStruct& NPCData : Spawns)
	{
		SpawnNPC(NPCData);
	}
}

void UNPCManager::CleanupInvalidNPCs()
{
	TArray<int32> InvalidNPCIds;
	
	for (const auto& NPCPair : SpawnedNPCs)
	{
		if (!IsValid(NPCPair.Value))
		{
			InvalidNPCIds.Add(NPCPair.Key);
		}
	}
	
	for (int32 InvalidId : InvalidNPCIds)
	{
		SpawnedNPCs.Remove(InvalidId);
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: Cleaned up invalid NPC with ID: %d"), InvalidId);
	}
	
	if (InvalidNPCIds.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("NPCManager: Cleaned up %d invalid NPCs"), InvalidNPCIds.Num());
	}
}