#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "UI/InventoryWidget.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "CrashDiagnostics.h"

UInventoryManager::UInventoryManager()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize references
	networkManager = nullptr;
	gameInstance = nullptr;
	worldContext = nullptr;
	bInventoryLoaded = false;

	// Initialize inventory
	CurrentInventory = FCharacterInventoryStruct();
}

void UInventoryManager::BeginPlay()
{
	Super::BeginPlay();
}

void UInventoryManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UInventoryManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("InventoryManager: GameInstance found"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("InventoryManager: GameInstance not found"));
		}
	}
}

void UInventoryManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

void UInventoryManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UInventoryManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Network Manager found and subscribed to events"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Network Manager is valid"));

			// Subscribe to the network manager's events
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UInventoryManager::ProcessGameServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UInventoryManager::ProcessGameServerData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("InventoryManager: Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Network manager not found"));
	}
}

