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

// Add receive ping time
void UAuthenticationManager::AddReceivePingTime(const FString& EventType, const FDateTime& ReceiveTime)
{
	// Add the received timestamp to the array, indexed by event type
	if (!ReceiveTimes.Contains(EventType))
	{
		ReceiveTimes.Add(EventType, TArray<FDateTime>());
	}
	ReceiveTimes[EventType].Add(ReceiveTime);

	// Check if there are sufficient send and receive timestamps for this event type
	if (SendTimes.Contains(EventType) && ReceiveTimes.Contains(EventType) &&
		SendTimes[EventType].Num() >= PingPacketsCount && ReceiveTimes[EventType].Num() >= PingPacketsCount &&
		pingManager)
	{
		// Calculate average ping time using the send and receive timestamps for this event type
		pingManager->CalculatePingTime(SendTimes[EventType], ReceiveTimes[EventType], "Login Server");

		// Clear the arrays to start collecting new values (assuming one-to-one correspondence)
		SendTimes[EventType].Empty();
		ReceiveTimes[EventType].Empty();
	}
}

// Add send ping time
void UAuthenticationManager::AddSendPingTime(const FString& EventType, const FDateTime& SendTime)
{
	// Add the send timestamp to the array, indexed by event type
	if (!SendTimes.Contains(EventType))
	{
		SendTimes.Add(EventType, TArray<FDateTime>());
	}
	SendTimes[EventType].Add(SendTime);
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

	FString JsonString = JSONParser::SerializeJson("authentificationClient", HeaderData, BodyData);

	if(networkManager)
	{
		//if login and password are valid
		if (IsLoginValueValid(Username) && IsPasswordValueValid(Password))
		{
			if (networkManager != nullptr)
			{
				// add send time
				//AddSendPingTime("authentificationClient", FDateTime::UtcNow());

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

	FString JsonString = JSONParser::SerializeJson("getCharactersList", HeaderData, BodyData);

	if (networkManager != nullptr)
	{
		// add send time
		//AddSendPingTime("getCharactersList", FDateTime::UtcNow());

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

	FString JsonString = JSONParser::SerializeJson("disconnectClient", HeaderData, BodyData);

	if (networkManager != nullptr)
	{
		// add send time
		//AddSendPingTime("disconnectClient", FDateTime::UtcNow());

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

	if (MessageData.eventType == "authentificationClient" && MessageData.status == "success" && ClientData.clientId != 0 && ClientData.hash != "") {
		if (gameInstance) {
			gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);
			gameInstance->SetCurrentClientID(ClientData.clientId);
			gameInstance->SetCurrentClientHash(ClientData.hash);
			// Send the character list request
			SendCharacterListRequest(ClientData);
		}
	}
	else if(MessageData.eventType == "authentificationClient" && MessageData.status == "error"){
		gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);
		UE_LOG(LogTemp, Error, TEXT("Login failed"));
	}

	// if event type is get character list
	if (MessageData.eventType == "getCharactersList" && MessageData.status == "success") {
		// Deserialize the received JSON string to get characters list
		TArray<FCharacterDataStruct> CharactersList = JSONParser::DeserializeLoginCharactersList(ReceivedData);
		// Set the characters list to the login screen UI
		if (gameInstance) {
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), MessageData.message);
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

	// If the ping manager is valid, calculate the ping time
	if (pingManager != nullptr && MessageData.eventType == "pingClient") {
		// Process the received data here
		AddReceivePingTime(MessageData.eventType, FDateTime::UtcNow());
	}
}

void UAuthenticationManager::SendPingRequest()
{
	// Create the header JSON object
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "pingClient");

	// Create the main JSON object and add header and body
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);

	// Serialize the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);


	if (networkManager != nullptr)
	{
		// add send time
		AddSendPingTime("pingClient", FDateTime::UtcNow());

		networkManager->SendDataToLoginServer(OutputString);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Network Manager not found"));
	}
}

void UAuthenticationManager::StartPing()
{
	if (worldContext)
	{
		worldContext->GetTimerManager().ClearTimer(NetworkServersPingTimerHandle);
		worldContext->GetTimerManager().SetTimer(NetworkServersPingTimerHandle, this, &UAuthenticationManager::SendPingRequest, PingTimeout, true);
		UE_LOG(LogTemp, Warning, TEXT("Ping timer for Login server set up successfully."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GetWorld() returned nullptr, ping timer for servers not set."));
	}
}