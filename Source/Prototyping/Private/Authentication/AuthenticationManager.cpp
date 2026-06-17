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

// ─────────────────────────────────────────────────────────────────────────────
// Request timeout helpers
// ─────────────────────────────────────────────────────────────────────────────

void UAuthenticationManager::StartRequestTimeout(FTimerHandle& Handle, void (UAuthenticationManager::* Callback)())
{
	UWorld* World = worldContext ? worldContext : (gameInstance ? gameInstance->GetWorld() : nullptr);
	if (!World) return;

	World->GetTimerManager().ClearTimer(Handle);
	World->GetTimerManager().SetTimer(Handle, this, Callback, RequestTimeoutSeconds, false);
}

void UAuthenticationManager::ClearRequestTimeout(FTimerHandle& Handle)
{
	UWorld* World = worldContext ? worldContext : (gameInstance ? gameInstance->GetWorld() : nullptr);
	if (!World) return;

	World->GetTimerManager().ClearTimer(Handle);
}

void UAuthenticationManager::OnLoginRequestTimeout()
{
	UE_LOG(LogTemp, Error, TEXT("AuthManager: Login request timed out after %.0fs — broadcasting error"), RequestTimeoutSeconds);
	OnLoginResponse.Broadcast(false, TEXT("ERR_TIMEOUT"));
}

void UAuthenticationManager::OnRegisterRequestTimeout()
{
	UE_LOG(LogTemp, Error, TEXT("AuthManager: Register request timed out after %.0fs — broadcasting error"), RequestTimeoutSeconds);
	OnRegisterResponse.Broadcast(false, TEXT("ERR_TIMEOUT"));
}



void UAuthenticationManager::FillAuthHeader(TMap<FString, TSharedPtr<FJsonValue>>& HeaderData) const
{
	if (!gameInstance) return;

	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID())));
	HeaderData.Add("hash",     MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash())));
}

bool UAuthenticationManager::HandleUnauthorized(const FString& Status, const FString& Message)
{
	if (Status == "error" && Message == "Unauthorized")
	{
		UE_LOG(LogTemp, Warning, TEXT("AuthManager: Session expired (Unauthorized)"));
		OnSessionExpired.Broadcast();
		return true;
	}
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Validation (legacy — kept for backward compat)
// ─────────────────────────────────────────────────────────────────────────────

bool UAuthenticationManager::IsLoginValueValid(const FString& Login)
{
	if (Login.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Login could not be empty!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login could not be empty!");
		}
		return false;
	}
	if (Login.Len() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login is too short!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login is too short!");
		}
		return false;
	}
	if (Login.Len() > 20)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login is too long!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login is too long!");
		}
		return false;
	}
	return true;
}

bool UAuthenticationManager::IsPasswordValueValid(const FString& Password)
{
	if (Password.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Password could not be empty!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password could not be empty!");
		}
		return false;
	}
	// FIXED: server expects 8–100 chars, not 3–20
	if (Password.Len() < 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("Password is too short!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password must be at least 8 characters!");
		}
		return false;
	}
	if (Password.Len() > 100)
	{
		UE_LOG(LogTemp, Warning, TEXT("Password is too long!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password must be 100 characters or fewer!");
		}
		return false;
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Outgoing requests
// ─────────────────────────────────────────────────────────────────────────────

void UAuthenticationManager::SendLoginRequest(const FString& Username, const FString& Password)
{
	UE_LOG(LogTemp, Warning, TEXT("AuthManager::SendLoginRequest — networkManager=%s, connected=%s, user='%s'"),
		networkManager ? TEXT("valid") : TEXT("NULL"),
		(networkManager && networkManager->IsLoginServerConnected()) ? TEXT("yes") : TEXT("no"),
		*Username);

	if (!networkManager || !networkManager->IsLoginServerConnected())
	{
		UE_LOG(LogTemp, Error, TEXT("AuthManager::SendLoginRequest — login server not connected, broadcasting error"));
		OnLoginResponse.Broadcast(false, TEXT("ERR_SERVER_UNAVAILABLE"));
		return;
	}

	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	BodyData.Add("login",    MakeShareable(new FJsonValueString(Username)));
	BodyData.Add("password", MakeShareable(new FJsonValueString(Password)));
	BodyData.Add("clientVersion", MakeShareable(new FJsonValueString(
		gameInstance ? gameInstance->ClientVersion : TEXT("0.1.0"))));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"authentificationClient", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	UE_LOG(LogTemp, Warning, TEXT("AuthManager::SendLoginRequest — sending JSON: %s"), *JsonString);
	networkManager->SendDataToLoginServer(JsonString);
	StartRequestTimeout(LoginRequestTimeoutHandle, &UAuthenticationManager::OnLoginRequestTimeout);
}

void UAuthenticationManager::SendRegisterRequest(const FString& Username, const FString& Password, const FString& Email)
{
	UE_LOG(LogTemp, Warning, TEXT("AuthManager::SendRegisterRequest — networkManager=%s, connected=%s, user='%s'"),
		networkManager ? TEXT("valid") : TEXT("NULL"),
		(networkManager && networkManager->IsLoginServerConnected()) ? TEXT("yes") : TEXT("no"),
		*Username);

	if (!networkManager || !networkManager->IsLoginServerConnected())
	{
		UE_LOG(LogTemp, Error, TEXT("AuthManager::SendRegisterRequest — login server not connected, broadcasting error"));
		OnRegisterResponse.Broadcast(false, TEXT("ERR_SERVER_UNAVAILABLE"));
		return;
	}

	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	BodyData.Add("login",    MakeShareable(new FJsonValueString(Username)));
	BodyData.Add("password", MakeShareable(new FJsonValueString(Password)));
	BodyData.Add("clientVersion", MakeShareable(new FJsonValueString(
		gameInstance ? gameInstance->ClientVersion : TEXT("0.1.0"))));
	if (!Email.IsEmpty())
	{
		BodyData.Add("email", MakeShareable(new FJsonValueString(Email)));
	}

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"registerAccount", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	UE_LOG(LogTemp, Warning, TEXT("AuthManager::SendRegisterRequest — sending JSON: %s"), *JsonString);
	networkManager->SendDataToLoginServer(JsonString);
	StartRequestTimeout(RegisterRequestTimeoutHandle, &UAuthenticationManager::OnRegisterRequestTimeout);
}

void UAuthenticationManager::SendCharacterCreationOptionsRequest()
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	FillAuthHeader(HeaderData);
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"getCharacterCreationOptions", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager)
	{
		networkManager->SendDataToLoginServer(JsonString);
	}
}

void UAuthenticationManager::SendCharacterListRequest(FClientDataStruct& ClientData)
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(ClientData.clientId)));
	HeaderData.Add("hash",     MakeShareable(new FJsonValueString(ClientData.hash)));

	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"getCharactersList", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager)
	{
		networkManager->SendDataToLoginServer(JsonString);
	}
}

void UAuthenticationManager::SendCreateCharacterRequest(
	const FString& CharacterName, const FString& CharacterClass,
	const FString& CharacterRace, const FString& CharacterGender)
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	FillAuthHeader(HeaderData);

	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	BodyData.Add("characterName",   MakeShareable(new FJsonValueString(CharacterName)));
	BodyData.Add("characterClass",  MakeShareable(new FJsonValueString(CharacterClass)));
	BodyData.Add("characterRace",   MakeShareable(new FJsonValueString(CharacterRace)));
	BodyData.Add("characterGender", MakeShareable(new FJsonValueString(CharacterGender)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"createCharacter", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager)
	{
		networkManager->SendDataToLoginServer(JsonString);
	}
}

void UAuthenticationManager::SendDeleteCharacterRequest(int32 CharacterId)
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	FillAuthHeader(HeaderData);

	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(CharacterId)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"deleteCharacter", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager)
	{
		networkManager->SendDataToLoginServer(JsonString);
	}
}

void UAuthenticationManager::SendLeaveGameRequest(FClientDataStruct& ClientData)
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(ClientData.clientId)));
	HeaderData.Add("hash",     MakeShareable(new FJsonValueString(ClientData.hash)));

	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(ClientData.characterData.characterId)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		"disconnectClient", HeaderData, BodyData,
		gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager)
	{
		networkManager->SendDataToLoginServer(JsonString);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ProcessLoginResponse — central dispatch for all login-server messages
// ─────────────────────────────────────────────────────────────────────────────

void UAuthenticationManager::ProcessLoginResponse(const FString& ReceivedData)
{
	UE_LOG(LogTemp, Warning, TEXT("Login Server Response: %s"), *ReceivedData);

	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	FClientDataStruct ClientData   = JSONParser::DeserializeClientData(ReceivedData);

	// ── Global Unauthorized check ────────────────────────────────────────────
	if (HandleUnauthorized(MessageData.status, MessageData.message))
	{
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// authentificationClient (Login)
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "authentificationClient")
	{
		ClearRequestTimeout(LoginRequestTimeoutHandle);

		if (MessageData.message == TEXT("ERR_VERSION_OUTDATED") || MessageData.message == TEXT("ERR_VERSION_TOO_NEW"))
		{
			UE_LOG(LogTemp, Warning, TEXT("Version mismatch: %s"), *MessageData.message);
			OnVersionMismatch.Broadcast(MessageData.message);
			return;
		}

		if (MessageData.status == "success" && ClientData.clientId != 0 && !ClientData.hash.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Login Success: Client ID: %d, Hash: %s"), ClientData.clientId, *ClientData.hash);

			if (gameInstance)
			{
				gameInstance->SetCurrentClientID(ClientData.clientId);
				gameInstance->SetCurrentClientHash(ClientData.hash);
			}

			OnLoginResponse.Broadcast(true, MessageData.message);

			// Auto-request character list + creation options
			SendCharacterListRequest(ClientData);
			SendCharacterCreationOptionsRequest();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Login Error: %s"), *MessageData.message);
			OnLoginResponse.Broadcast(false, MessageData.message);
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// registerAccount
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "registerAccount")
	{
		ClearRequestTimeout(RegisterRequestTimeoutHandle);

		if (MessageData.message == TEXT("ERR_VERSION_OUTDATED") || MessageData.message == TEXT("ERR_VERSION_TOO_NEW"))
		{
			UE_LOG(LogTemp, Warning, TEXT("Version mismatch: %s"), *MessageData.message);
			OnVersionMismatch.Broadcast(MessageData.message);
			return;
		}

		if (MessageData.status == "success" && ClientData.clientId != 0 && !ClientData.hash.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Registration Success: Client ID: %d"), ClientData.clientId);

			if (gameInstance)
			{
				gameInstance->SetCurrentClientID(ClientData.clientId);
				gameInstance->SetCurrentClientHash(ClientData.hash);
			}

			OnRegisterResponse.Broadcast(true, MessageData.message);

			// Auto-request character list + creation options
			SendCharacterListRequest(ClientData);
			SendCharacterCreationOptionsRequest();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Registration Error: %s"), *MessageData.message);
			OnRegisterResponse.Broadcast(false, MessageData.message);
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// getCharactersList
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "getCharactersList")
	{
		if (MessageData.status == "success")
		{
			TArray<FLoginCharacterEntry> Entries = JSONParser::DeserializeLoginCharactersList(ReceivedData);
			OnCharacterListReceived.Broadcast(Entries);

			// Legacy compat: also populate via GameInstance ListView
			if (gameInstance)
			{
				gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);

				// Build minimal FCharacterDataStruct list for legacy SetCharacterItems (uses only name + id)
				TArray<FCharacterDataStruct> LegacyList;
				LegacyList.Reserve(Entries.Num());
				for (const FLoginCharacterEntry& E : Entries)
				{
					FCharacterDataStruct D;
					D.characterId   = E.CharacterId;
					D.characterName = E.CharacterName;
					LegacyList.Add(MoveTemp(D));
				}
				gameInstance->SetCharacterItems(LegacyList);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Character list request failed: %s"), *MessageData.message);
			if (gameInstance)
			{
				gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);
			}
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// getCharacterCreationOptions
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "getCharacterCreationOptions")
	{
		if (MessageData.status == "success")
		{
			FCharacterCreationOptions Options = JSONParser::DeserializeCharacterCreationOptions(ReceivedData);
			UE_LOG(LogTemp, Warning, TEXT("Creation Options received: %d classes, %d races, %d genders"),
				Options.Classes.Num(), Options.Races.Num(), Options.Genders.Num());
			OnCharacterCreationOptionsReceived.Broadcast(Options);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Creation options request failed: %s"), *MessageData.message);
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// createCharacter
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "createCharacter")
	{
		if (MessageData.status == "success")
		{
			// Parse characterId from body
			int32 NewCharId = 0;
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
				if (JsonObj->TryGetObjectField(TEXT("body"), BodyPtr) && BodyPtr)
				{
					(*BodyPtr)->TryGetNumberField(TEXT("characterId"), NewCharId);
				}
			}

			UE_LOG(LogTemp, Warning, TEXT("Character created: ID=%d"), NewCharId);
			OnCreateCharacterResponse.Broadcast(true, MessageData.message, NewCharId);

			// Re-request character list to refresh UI
			if (gameInstance)
			{
				FClientDataStruct CD;
				CD.clientId = gameInstance->GetCurrentClientID();
				CD.hash     = gameInstance->GetCurrentClientHash();
				SendCharacterListRequest(CD);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Character creation failed: %s"), *MessageData.message);
			OnCreateCharacterResponse.Broadcast(false, MessageData.message, 0);
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// deleteCharacter
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "deleteCharacter")
	{
		int32 DeletedCharId = 0;
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
			if (JsonObj->TryGetObjectField(TEXT("body"), BodyPtr) && BodyPtr)
			{
				(*BodyPtr)->TryGetNumberField(TEXT("characterId"), DeletedCharId);
			}
		}

		if (MessageData.status == "success")
		{
			UE_LOG(LogTemp, Warning, TEXT("Character deleted: ID=%d"), DeletedCharId);
			OnDeleteCharacterResponse.Broadcast(true, MessageData.message, DeletedCharId);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Character deletion failed: %s"), *MessageData.message);
			OnDeleteCharacterResponse.Broadcast(false, MessageData.message, DeletedCharId);
		}
		return;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Ping
// ─────────────────────────────────────────────────────────────────────────────

void UAuthenticationManager::StartPing()
{
	if (worldContext && pingManager)
	{
		pingManager->SetWorldContext(worldContext);
		pingManager->StartPingUpdates();
		UE_LOG(LogTemp, Warning, TEXT("AuthenticationManager: Started TimeSyncService-based ping updates"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AuthenticationManager: Cannot start ping - missing worldContext or pingManager"));
	}
}