// Fill out your copyright notice in the Description page of Project Settings.

#include "Authentication/AuthenticationManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"

void UAuthenticationManager::ProcessLoginPacket(const FParsedPacket& Packet)
{
	// Compat alias: the login-flow Deserialize* helpers below have no Root
	// overloads (S05b converts them) — kept for those branches only.
	const FString& ReceivedData = Packet.Raw;

	UE_LOG(LogNetPacket, Verbose, TEXT("Login Server Response: %s"), *ReceivedData);

	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(Packet);
	FClientDataStruct ClientData   = JSONParser::DeserializeClientData(Packet.Root);

	// ── Global Unauthorized check ────────────────────────────────────────────
	if (HandleUnauthorized(MessageData.status, MessageData.message, MessageData.errorCode))
	{
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// authentificationClient (Login)
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "authentificationClient")
	{
		ClearRequestTimeout(LoginRequestTimeoutHandle);

		if (MessageData.errorCode == TEXT("ERR_VERSION_OUTDATED") || MessageData.errorCode == TEXT("ERR_VERSION_TOO_NEW"))
		{
			UE_LOG(LogConnection, Warning, TEXT("Version mismatch: %s (server=%s min=%s max=%s)"), *MessageData.errorCode, *MessageData.serverVersion, *MessageData.minClientVersion, *MessageData.maxClientVersion);
			OnVersionMismatch.Broadcast(MessageData.errorCode);
			OnVersionMismatchDetailed.Broadcast(MessageData.errorCode, MessageData.minClientVersion, MessageData.maxClientVersion, MessageData.serverVersion);
			return;
		}

		if (MessageData.status == "success" && ClientData.clientId != 0 && !ClientData.hash.IsEmpty())
		{
			UE_LOG(LogConnection, Verbose, TEXT("Login Success: Client ID: %d"), ClientData.clientId);

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
			if (MessageData.errorCode == TEXT("ERR_ACCOUNT_BANNED"))
			{
				UE_LOG(LogConnection, Error, TEXT("Account banned: reason=%s expires=%s"), *MessageData.banReason, *MessageData.banExpiresAt);
				OnAccountBanned.Broadcast(MessageData.errorCode, MessageData.banReason, MessageData.banExpiresAt);
			}
			if (MessageData.errorCode == TEXT("ERR_DB"))
			{
				UE_LOG(LogConnection, Error, TEXT("Login Error: ERR_DB (transient — advise retry): %s"), *MessageData.message);
			}
			UE_LOG(LogConnection, Error, TEXT("Login Error: %s"), *MessageData.errorCode);
			OnLoginResponse.Broadcast(false, MessageData.errorCode);
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// registerAccount
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "registerAccount")
	{
		ClearRequestTimeout(RegisterRequestTimeoutHandle);

		if (MessageData.errorCode == TEXT("ERR_VERSION_OUTDATED") || MessageData.errorCode == TEXT("ERR_VERSION_TOO_NEW"))
		{
			UE_LOG(LogConnection, Warning, TEXT("Version mismatch: %s (server=%s min=%s max=%s)"), *MessageData.errorCode, *MessageData.serverVersion, *MessageData.minClientVersion, *MessageData.maxClientVersion);
			OnVersionMismatch.Broadcast(MessageData.errorCode);
			OnVersionMismatchDetailed.Broadcast(MessageData.errorCode, MessageData.minClientVersion, MessageData.maxClientVersion, MessageData.serverVersion);
			return;
		}

		if (MessageData.status == "success" && ClientData.clientId != 0 && !ClientData.hash.IsEmpty())
		{
			UE_LOG(LogConnection, Warning, TEXT("Registration Success: Client ID: %d"), ClientData.clientId);

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
			if (MessageData.errorCode == TEXT("ERR_ACCOUNT_BANNED"))
			{
				UE_LOG(LogConnection, Error, TEXT("Account banned: reason=%s expires=%s"), *MessageData.banReason, *MessageData.banExpiresAt);
				OnAccountBanned.Broadcast(MessageData.errorCode, MessageData.banReason, MessageData.banExpiresAt);
			}
			if (MessageData.errorCode == TEXT("ERR_DB"))
			{
				UE_LOG(LogConnection, Error, TEXT("Registration Error: ERR_DB (transient — advise retry): %s"), *MessageData.message);
			}
			UE_LOG(LogConnection, Error, TEXT("Registration Error: %s"), *MessageData.errorCode);
			OnRegisterResponse.Broadcast(false, MessageData.errorCode);
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

			// The login flow populates its own list from the delegate above;
			// no legacy ListView feed here (it only Error-spammed on a null widget).
			if (gameInstance)
			{
				gameInstance->OnLoginResponseReceived.Broadcast(ClientData.clientId, MessageData.message);
			}
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("Character list request failed: %s"), *MessageData.message);
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
			UE_LOG(LogConnection, Warning, TEXT("Creation Options received: %d classes, %d races, %d genders"),
				Options.Classes.Num(), Options.Races.Num(), Options.Genders.Num());
			OnCharacterCreationOptionsReceived.Broadcast(Options);
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("Creation options request failed: %s"), *MessageData.message);
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
			// Parse characterId from body (already parsed — no re-parse)
			int32 NewCharId = 0;
			if (Packet.Body.IsValid())
			{
				Packet.Body->TryGetNumberField(TEXT("characterId"), NewCharId);
			}

			UE_LOG(LogConnection, Warning, TEXT("Character created: ID=%d"), NewCharId);
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
			UE_LOG(LogConnection, Error, TEXT("Character creation failed: %s"), *MessageData.message);
			OnCreateCharacterResponse.Broadcast(false, MessageData.message, 0);
		}
		return;
	}

	// ══════════════════════════════════════════════════════════════════════════
	// deleteCharacter
	// ══════════════════════════════════════════════════════════════════════════
	if (MessageData.eventType == "deleteCharacter")
	{
		// Parse characterId from body (already parsed — no re-parse)
		int32 DeletedCharId = 0;
		if (Packet.Body.IsValid())
		{
			Packet.Body->TryGetNumberField(TEXT("characterId"), DeletedCharId);
		}

		if (MessageData.status == "success")
		{
			UE_LOG(LogConnection, Warning, TEXT("Character deleted: ID=%d"), DeletedCharId);
			OnDeleteCharacterResponse.Broadcast(true, MessageData.message, DeletedCharId);

			// Re-request character list to refresh the UI (mirrors createCharacter flow)
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
			UE_LOG(LogConnection, Error, TEXT("Character deletion failed: %s"), *MessageData.message);
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
		UE_LOG(LogConnection, Verbose, TEXT("AuthenticationManager: Started TimeSyncService-based ping updates"));
	}
	else
	{
		UE_LOG(LogConnection, Warning, TEXT("AuthenticationManager: Cannot start ping - missing worldContext or pingManager"));
	}
}
