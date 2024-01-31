#pragma once

#include "CoreMinimal.h"
#include <Kismet/GameplayStatics.h>
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "NetworkWorkers/NetworkReceiverWorker.h"
#include "NetworkWorkers/NetworkSenderWorker.h"
#include "Player/MyCameraActor.h"
#include "Player/BasicPlayer.h"
#include "Services/LoadingSceenActor.h"
#include "Widgets/LoginWidget.h"
#include "Widgets/CharacterListItem.h"
#include "Components/ListView.h"
#include "MyGameInstance.generated.h"


// Delegate to handle the login response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResponseReceived, int32, ClientID, const FString&, ResponseMessage);

// Delegate to handle the game server response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameServerResponseReceived, int32, ClientID, const FString&, ResponseMessage);

/**
 *
 */
UCLASS()
class PROTOTYPING_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

private:
	FTimerHandle NetworkLoginServerPollTimerHandle; // Timer handle for polling network data

	FTimerHandle NetworkGameServerPollTimerHandle; // Timer handle for polling network data

	FTimerHandle LoadLoginLevelTimerHandle; // Timer handle for loading the login level

	FTimerHandle RemoveLoadingScreenTimerHandle; // Timer handle for loading the game level

	// Client ID
	int32 CurrentClientID;

	// Client token
	FString CurrentClientSecret;

	// Client login
	FString CurrentClientLogin;

	// Client Character ID
	int32 CurrentCharacterID;




public:
	UMyGameInstance(const FObjectInitializer& ObjectInitializer);

	void Init();

	FString BoolToString(bool bValue);

	void Shutdown();

	void InitializeTCPConnection();

	void CleanupTCPConnection();

	TMap<int32, FPlayerData> ConnectedPlayers;

	TMap<int32, ABasicPlayer*> SpawnedPlayers;

	void AddPlayerData(int32 PlayerID, const FPlayerData& PlayerData);

	void RemovePlayerData(int32 PlayerID, const FPlayerData& PlayerData);

	void MovePlayerForClient(int32 ClientID, double x, double y, double z);

	void HandlePlayerDisconnection(int32 ClientID);

	void UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z);

	void GetCharacterItemsData(FString& clientSecret, int32& clientID);

	void SetCharacterItems(TArray<FCharacterItemData> Items);

	// spawn players
	void SpawnPlayerForClient(int32 ClientID);

	// Login server details
	FString LoginServerIP;
	int32 LoginServerPort;
	FSocket* LoginServerSocket;
	bool bIsLoginSocketConnected;

	NetworkReceiverWorker* ReceiverLoginServerWorker;
	FRunnableThread* ReceiverLoginServerThread;

	NetworkSenderWorker* SenderLoginServerWorker;
	FRunnableThread* SenderLoginServerThread;

	// Game server details
	FString GameServerIP;
	int32 GameServerPort;
	FSocket* GameServerSocket;
	bool bIsGameSocketConnected;

	NetworkReceiverWorker* ReceiverGameServerWorker;
	FRunnableThread* ReceiverGameServerThread;

	NetworkSenderWorker* SenderGameServerWorker;
	FRunnableThread* SenderGameServerThread;

	// Method to send data to the login server
	void SendLoginServerNetworkData(const FString& Data);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void JoinToLoginServer(const FString& Username, const FString& Password);

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnLoginResponseReceived OnLoginResponseReceived;

	// Method to poll for new data form login server
	void PollLoginServerNetworkData();

	void ProcessLoginResponse(const FString& ResponseMessage); // Function to process loign server response

	// Method to send data to the game server
	void SendGameServerNetworkData(const FString& Data);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void JoinToGameServer(const int32& characterID);

	void GetConnectedPlayers(FString& clientSecret, int32& clientID, int32& characterID);

	// Player Movement To Game Server
	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendPlayerMovementToGameServer(FString& clientSecret, int32& clientID, int32& characterID, double& x, double& y, double& z);

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnLoginResponseReceived OnGameServerResponseReceived;

	// Method to poll for new data form game server
	void PollGameServerNetworkData();

	void ProcessGameServerResponse(const FString& ResponseMessage); // Function to process game server response


	UFUNCTION(BlueprintCallable, Category = "Level")
	void LoadLevel(const FName& LevelName);

	void LoadLoginLevel();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void OnLevelLoaded();

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FName GameLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	FName LoginLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;
	UUserWidget* LoadingScreenWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<ULoginWidget> LoginScreenWidgetClass;
	ULoginWidget* LoginScreenWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCharacterListItem> CharactersListItemWidgetClass;

	// A member variable to store the name of the level being loaded
	FName LevelBeingLoaded;

	// Camera actor for the login level
	AMyCameraActor* LoginLevelCamera;

	// Player actor for the game level
	ABasicPlayer* Player;

	// add a reference to the player actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TSubclassOf<class ABasicPlayer> MainPlayerClass;

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

	// Loading screen actor
	ALoadingSceenActor* LoadingScreenActor;
};
