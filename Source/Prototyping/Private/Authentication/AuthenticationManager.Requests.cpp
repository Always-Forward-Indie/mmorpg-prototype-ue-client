// Fill out your copyright notice in the Description page of Project Settings.

#include "Authentication/AuthenticationManager.h"
#include "MyGameInstance.h"

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

