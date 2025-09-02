#pragma once

#include "CoreMinimal.h"

#include "Networking/NetworkManager.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/SpawnZoneManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "UI/UIManager.h"
#include "Services/TimeSyncService.h"

#include <Kismet/GameplayStatics.h>
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h" 

#include "Gameplay/Players/MyCameraActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Services/LoadingSceenActor.h"
#include "Gameplay/UI/LoginWidget.h"
#include "Gameplay/UI/CharacterListItem.h"
#include "Gameplay/UI/MonitorStatsWidget.h"
#include "Components/ListView.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.generated.h"


// Delegate to handle the login response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResponseReceived, int32, ClientID, const FString&, ResponseMessage);

// Delegate to handle the game server response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameServerResponseReceived, int32, ClientID, const FString&, ResponseMessage);

/**
 *
 */

class UFloatingCombatTextManager;
class ADroppedItemActor;

UCLASS()
class PROTOTYPING_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

private:
	FTimerHandle LoadLoginLevelTimerHandle; // Timer handle for loading the login level

	FTimerHandle RemoveLoadingScreenTimerHandle; // Timer handle for loading the game level

	FTimerHandle NetworkServersPingTimerHandle; // Timer handle for ping game server

	// ClientData
	FClientDataStruct ClientData;

	// Client ID
	int32 CurrentClientID;

	// Client token
	FString CurrentClientSecret;

	// Client login
	FString CurrentClientLogin;

	// Client Character ID
	int32 CurrentCharacterID;

	// Time variables to measure ping
	FDateTime SendTimeGameServer;
	FDateTime SendTimeLoginServer;
	FDateTime ReceiveTimeGameServer;
	FDateTime ReceiveTimeLoginServer;

public:
	UMyGameInstance(const FObjectInitializer& ObjectInitializer);

	void Init();

	void InitNetworkingSetup();

	void InitGameSystems();

	void Shutdown();

	UPROPERTY()
	// Network manager
	UNetworkManager* NetworkManager;

	UPROPERTY()
	// Ping manager
	UPingManager* PingManager;

	UPROPERTY()
	// Authentication manager
	UAuthenticationManager* AuthenticationManager;

	UPROPERTY()
	// Player manager
	UPlayerManager* PlayerManager;

	UPROPERTY()
	// MOB manager
	UMOBManager* MOBManager;

	// Zone manager
	UPROPERTY()
	USpawnZoneManager* SpawnZoneManager;

	// Item manager
	UPROPERTY()
	UItemManager* ItemManager;

	// Inventory manager
	UPROPERTY()
	UInventoryManager* InventoryManager;

	// Harvest manager
	UPROPERTY()
	class UHarvestManager* HarvestManager;

	// Experience manager
	UPROPERTY()
	class UExperienceManager* ExperienceManager;

	// Experience network handler
	UPROPERTY()
	class UExperienceNetworkHandler* ExperienceNetworkHandler;

	// Combat system managers
	UPROPERTY()
	class UCombatSystemManager* CombatSystemManager;

	UPROPERTY()
	class USkillSystemManager* SkillSystemManager;

	UPROPERTY()
	class UCombatNetworkHandler* CombatNetworkHandler;

	// Time synchronization service
	UPROPERTY()
	UTimeSyncService* TimeSyncService;

	TMap<int32, FClientDataStruct> ConnectedPlayers;

	TMap<int32, ABasicPlayer*> SpawnedPlayers;

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnLoginResponseReceived OnLoginResponseReceived;

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnGameServerResponseReceived OnGameServerResponseReceived;

	// get network manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UNetworkManager* GetNetworkManager();

	// get Authentication manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UAuthenticationManager* GetAuthenticationManager();

	// get Player manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UPlayerManager* GetPlayerManager();

	// get MOB manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UMOBManager* GetMOBManager();

	// get baic mob class
	UFUNCTION(BlueprintCallable, Category = "MOB")
	TSubclassOf<class ABasicMOB> GetBasicMOBClass();

	// get spawn zone manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	USpawnZoneManager* GetSpawnZoneManager();

	// get basic SpawnZone class
	UFUNCTION(BlueprintCallable, Category = "MOB")
	TSubclassOf<class AMobSpawnZone> GetBasicSpawnZoneClass();

	// get dropped item actor class
	UFUNCTION(BlueprintCallable, Category = "Items")
	TSubclassOf<class ADroppedItemActor> GetDroppedItemActorClass();

	// get item manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UItemManager* GetItemManager();

	// get inventory manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UInventoryManager* GetInventoryManager();

	// get harvest manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	class UHarvestManager* GetHarvestManager();

	// get experience manager
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UExperienceManager* GetExperienceManager();

	// get experience network handler
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UExperienceNetworkHandler* GetExperienceNetworkHandler();

	// get combat system manager
	UFUNCTION(BlueprintCallable, Category = "Combat")
	class UCombatSystemManager* GetCombatSystemManager();

	// get skill system manager
	UFUNCTION(BlueprintCallable, Category = "Combat")
	class USkillSystemManager* GetSkillSystemManager();

	// get combat network handler
	UFUNCTION(BlueprintCallable, Category = "Combat")
	class UCombatNetworkHandler* GetCombatNetworkHandler();

	// get current client data
	UFUNCTION(BlueprintCallable, Category = "Client Data")
	FClientDataStruct GetCurrentClientData();

	// UI manager
	UPROPERTY()
	UUIManager* UIManager;

	UUIManager* GetUIManager();

	void SetCurrentClientID(int32 ClientID);

	int32 GetCurrentClientID();

	void SetCurrentClientHash(FString ClientSecret);

	FString GetCurrentClientHash();

	void AddPlayerData(int32 ClientID, const FClientDataStruct clientData);

	void RemovePlayerData(int32 ClientID);

	void MovePlayerForClient(const int32 ClientID, const FClientDataStruct& clientData, const FMessageDataStruct& MessageData);

	void HandlePlayerDisconnection(int32 ClientID);

	void UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z, double rotZ);

	void SetCharacterItems(TArray<FCharacterDataStruct> Items);

	// spawn players
	void SpawnPlayerForClient(int32 ClientID);

	UFUNCTION(BlueprintCallable, Category = "Level")
	void LoadLevel(const FName& LevelName);

	void LoadLoginLevel();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void OnLevelLoaded();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void OnLevelUnloaded();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void AddMonitorStatsWidgetToViewport();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void AddLoginWidgetToViewport();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RemoveLoginWidgetFromViewport();

	void AddLoadingScreen();

	void RemoveLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "Character")
	void SetCurrentCharacterID(int32 CharacterID);

	UFUNCTION(BlueprintCallable, Category = "Character")
	int32 GetCurrentCharacterID();

	// Join the selected character to the game
	UFUNCTION(BlueprintCallable, Category = "Game")
	void JoinSelectedCharacterToGame();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName GameLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName LoginLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName DebugLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;
	UUserWidget* LoadingScreenWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<ULoginWidget> LoginScreenWidgetClass;
	ULoginWidget* LoginScreenWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCharacterListItem> CharactersListItemWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UMonitorStatsWidget> MonitorStatsWidgetClass;
	UMonitorStatsWidget* MonitorStatsWidget;

	// Класс виджета, который создаётся в Blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UMessageBoxPopup> MessageBoxPopupClass;


	// A member variable to store the name of the level being loaded
	FName LevelBeingLoaded;

	// Camera actor for the login level
	AMyCameraActor* LoginLevelCamera;

	// Player actor for the game level
	ABasicPlayer* Player;

	// add a reference to the player actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TSubclassOf<class ABasicPlayer> MainPlayerClass;

	// add a reference to the MOB actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOB")
	TSubclassOf<class ABasicMOB> BasicMOBClass;

	// add a reference to the spawn zone actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOB")
	TSubclassOf<class AMobSpawnZone> BasicSpawnZoneClass;

	// add a reference to the dropped item actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	TSubclassOf<class ADroppedItemActor> DroppedItemActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	UDataTable* ItemVisualsDataTable;

	// add a reference to the camera actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TSubclassOf<class AMyCameraActor> LoginCameraClass;

	// camera rotation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FRotator LoginLevelCameraRotation; // -15.0f, 235.0f, 0.0f

	// camera location
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FVector LoginLevelCameraLocation; //1675.0f, -190.0f, 605.0f

	// player location
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FVector GamePlayerLocation;

	// player rotation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FRotator GamePlayerRotation; // 0.0f, 0.0f, 0.0f

	// sound cue for the login screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* LoginMusicSoundSource;

	// sound cue for the loading screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* LoadingMusicSoundSource;

	// debug flag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebug;

	// Loading screen actor
	ALoadingSceenActor* LoadingScreenActor;

	// Ping servers
	UFUNCTION()
	void StartPingGameServer();
	UFUNCTION()
	void StartPingLoginServer();

	public:
		// Combat system functions (updated for new system)
		UFUNCTION(BlueprintCallable, Category = "Combat")
		void PlayCombatAnimation(const FCombatAnimationData& AnimationData);

		UFUNCTION(BlueprintCallable, Category = "Combat")
		void ProcessCombatAction(const FCombatActionData& ActionData);

		// Health update methods (supporting both new and legacy systems)
		UFUNCTION(BlueprintCallable, Category = "Combat")
		void UpdateMobHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt);

		UFUNCTION(BlueprintCallable, Category = "Combat")
		void UpdatePlayerHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt);

		UFUNCTION(BlueprintCallable, Category = "Combat")
		void UpdateTargetHealth(int32 TargetId, int32 TargetType, const FString& TargetTypeString, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt);

		// Set inventory manager reference
		UFUNCTION(BlueprintCallable, Category = "Network")
		void SetInventoryManager(UInventoryManager* NewInventoryManager);

		// get time sync service
		UFUNCTION(BlueprintCallable, Category = "Network")
		UTimeSyncService* GetTimeSyncService();

		// Process time sync data from server responses
		UFUNCTION(BlueprintCallable, Category = "Network")
		void ProcessTimeSyncData(const FMessageDataStruct& MessageData);

		// Get player by character ID
		UFUNCTION(BlueprintCallable, Category = "Player")
		ABasicPlayer* GetPlayerByCharacterId(int32 CharacterId);
};
