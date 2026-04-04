// Fill out your copyright notice in the Description page of Project Settings.

#include "Authentication/AuthenticationManager.h"
#include "MyGameInstance.h"

UAuthenticationManager::UAuthenticationManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// initialize the authentication manager
void UAuthenticationManager::Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager)
{
	// Initialize the network manager
	networkManager = NetworkManager;

	// Initialize the ping manager
	pingManager = PingManager;

	// Get the game instance
	gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

	if (gameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameInstance found"));
		
		// Set up PingManager with TimeSyncService integration
		if (pingManager)
		{
			pingManager->SetTimeSyncService(gameInstance->GetTimeSyncService());
			UE_LOG(LogTemp, Warning, TEXT("AuthenticationManager: PingManager configured with TimeSyncService"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameInstance not found"));
	}

	// Subscribe to the network manager's event
	//SubscribeToNetworkManager();
}

//subscribe to network manager
void UAuthenticationManager::SubscribeToNetworkManager()
{
	// Subscribe to the network manager's event
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Network Manager found and subscribed to LoginServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("Network Manager is valid"));
			networkManager->OnLoginServerDataReceived.RemoveDynamic(this, &UAuthenticationManager::ProcessLoginResponse);
			networkManager->OnLoginServerDataReceived.AddDynamic(this, &UAuthenticationManager::ProcessLoginResponse);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Network Manager is not valid"));
		}

	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Network Manager not found"));
	}
}

// set world context 
void UAuthenticationManager::SetWorldContext(UWorld* WorldContext)
{
	// Set the world context
	worldContext = WorldContext;
}

// is login value valid function
bool UAuthenticationManager::IsLoginValueValid(const FString& Login)
{
	//if login is empty
	if (Login.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Login could not be empty!"));
		if (gameInstance)
		{
			// message to the login screen
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login could not be empty!");
		}

		return false;
	}

	//if login is too short
	if (Login.Len() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login is too short!"));

		if (gameInstance)
		{
			// message to the login screen
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login is too short!");
		}

		return false;
	}

	//if login is too long
	if (Login.Len() > 20)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login is too long!"));

		if (gameInstance)
		{
			// message to the login screen
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login is too long!");
		}

		return false;
	}

	return true;
}

// is password value valid function
bool UAuthenticationManager::IsPasswordValueValid(const FString& Password)
{
	//if password is empty
	if (Password.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Password could not be empty!"));
		if (gameInstance)
		{
			// message to the login screen
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password could not be empty!");
		}

		return false;
	}

	//if password is too short
	if (Password.Len() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Password is too short!"));

		if (gameInstance)
		{
			// message to the login screen
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password is too short!");
		}

		return false;
	}

	//if password is too long
	if (Password.Len() > 20)
	{
		UE_LOG(LogTemp, Warning, TEXT("Password is too long!"));

		if (gameInstance)
		{
			// message to the login screen
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password is too long!");
		}

		return false;
	}

	return true;
}

// send login request to the server
void UAuthenticationManager::SendLoginRequest(const FString& Username, const FString& Password)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	TSharedPtr<FJsonValueString> UsernameValue = MakeShareable(new FJsonValueString(Username));
	TSharedPtr<FJsonValueString> PasswordValue = MakeShareable(new FJsonValueString(Password));

	BodyData.Add("login", UsernameValue);
	BodyData.Add("password", PasswordValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("authentificationClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if(networkManager)
	{
		//if login and password are valid
		if (IsLoginValueValid(Username) && IsPasswordValueValid(Password))
		{
			if (networkManager != nullptr)
			{
				// Send the JSON string to the login server
				networkManager->SendDataToLoginServer(JsonString);
			}
		}
	}
}

// send character list request to the server
void UAuthenticationManager::SendCharacterListRequest(FClientDataStruct& ClientData)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("getCharactersList", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the login server
		networkManager->SendDataToLoginServer(JsonString);
	}
}

void UAuthenticationManager::SendLeaveGameRequest(FClientDataStruct& ClientData)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	// set character ID
	TSharedPtr<FJsonValueNumber> CharacterIDValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterId));

	BodyData.Add("characterId", CharacterIDValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("disconnectClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the login server
		networkManager->SendDataToLoginServer(JsonString);
	}
}

void UAuthenticationManager::ProcessLoginResponse(const FString& ReceivedData)
{
	UE_LOG(LogTemp, Warning, TEXT("Init by delegate Login Server: %s"), *ReceivedData);

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);

	// Deserialize the received JSON string to get ClientData struct
	FClientDataStruct ClientData = JSONParser::DeserializeClientData(ReceivedData);

	// If authentication is successful
	if (MessageData.eventType == "authentificationClient" && MessageData.status == "success" && ClientData.clientId != 0 && ClientData.hash != "") {
		if (gameInstance) {
			UE_LOG(LogTemp, Warning, TEXT("Login Success: Client ID: %d, Hash: %s"), ClientData.clientId, *ClientData.hash);

			// Set the client data
			gameInstance->SetCurrentClientID(ClientData.clientId);
			gameInstance->SetCurrentClientHash(ClientData.hash);

			// Request characters list
			SendCharacterListRequest(ClientData);
		}
	}

	// Handle authentication errors
	if (MessageData.eventType == "authentificationClient" && MessageData.status == "error") {
		UE_LOG(LogTemp, Error, TEXT("Login Error: %s"), *MessageData.message);
		// Handle error (show message to user, etc.)
	}

	// if event type is get character list
	if (MessageData.eventType == "getCharactersList" && MessageData.status == "success") {
		// Deserialize the received JSON string to get characters list
			TArray<FCharacterDataStruct> CharactersList = JSONParser::DeserializeLoginCharactersList(ReceivedData);
		// Set the characters list to the login screen UI
		if (gameInstance) {
			gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);
			// Set the character items
			gameInstance->SetCharacterItems(CharactersList);
			// Show the character selection screen
			gameInstance->LoginScreenWidget->ShowCharacterSelection();
		}
	}
	else if (MessageData.eventType == "getCharactersList" && MessageData.status == "error")
	{
		gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);
		UE_LOG(LogTemp, Error, TEXT("Character list request failed"));
	}

	// Legacy ping handling is no longer needed - TimeSyncService handles ping automatically
	// The old ping calculation code has been removed in favor of TimeSyncService integration
}

void UAuthenticationManager::StartPing()
{
	if (worldContext && pingManager)
	{
		// Set world context for PingManager
		pingManager->SetWorldContext(worldContext);
		
		// Start the new TimeSyncService-based ping updates
		pingManager->StartPingUpdates();
		
		UE_LOG(LogTemp, Warning, TEXT("AuthenticationManager: Started TimeSyncService-based ping updates"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AuthenticationManager: Cannot start ping - missing worldContext or pingManager"));
	}
}