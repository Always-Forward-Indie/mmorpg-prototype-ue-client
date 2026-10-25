// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// Deserialize network header with time sync data
FNetworkHeaderStruct JSONParser::DeserializeNetworkHeader(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return FNetworkHeaderStruct();
	}
	return DeserializeNetworkHeader(JsonObject);
}

FNetworkHeaderStruct JSONParser::DeserializeNetworkHeader(const TSharedPtr<FJsonObject>& JsonObject)
{
	FNetworkHeaderStruct NetworkHeader;
	if (!JsonObject.IsValid())
	{
		return NetworkHeader;
	}

	const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
        {
                (*HeaderObject)->TryGetStringField(TEXT("eventType"), NetworkHeader.eventType);

                (*HeaderObject)->TryGetStringField(TEXT("status"), NetworkHeader.status);

                (*HeaderObject)->TryGetNumberField(TEXT("clientId"), NetworkHeader.clientId);

                (*HeaderObject)->TryGetStringField(TEXT("hash"), NetworkHeader.hash);

                (*HeaderObject)->TryGetStringField(TEXT("message"), NetworkHeader.message);

            // Check for requestIdEcho first (server response), then requestId (client request)
            if (!(*HeaderObject)->TryGetStringField(TEXT("requestIdEcho"), NetworkHeader.requestId))
            {
                (*HeaderObject)->TryGetStringField(TEXT("requestId"), NetworkHeader.requestId);
            }

            // Time sync fields
            if (double TmpClientSendMs = 0.0; (*HeaderObject)->TryGetNumberField(TEXT("clientSendMs"), TmpClientSendMs))
                NetworkHeader.clientSendMs = static_cast<int64>(TmpClientSendMs);

            if (double TmpServerRecvMs = 0.0; (*HeaderObject)->TryGetNumberField(TEXT("serverRecvMs"), TmpServerRecvMs))
                NetworkHeader.serverRecvMs = static_cast<int64>(TmpServerRecvMs);

            if (double TmpServerSendMs = 0.0; (*HeaderObject)->TryGetNumberField(TEXT("serverSendMs"), TmpServerSendMs))
                NetworkHeader.serverSendMs = static_cast<int64>(TmpServerSendMs);

            if (double TmpClientSendMsEcho = 0.0; (*HeaderObject)->TryGetNumberField(TEXT("clientSendMsEcho"), TmpClientSendMsEcho))
                NetworkHeader.clientSendMsEcho = static_cast<int64>(TmpClientSendMsEcho);
        }

        // Per protocol (�1.4 pongClient), timestamps may be a root-level object
        // (sibling of "header") instead of fields inside "header". Check and override
        // if present so that pongClient responses are parsed correctly.
        const TSharedPtr<FJsonObject>* TimestampsObject = nullptr;
        if (JsonObject->TryGetObjectField(TEXT("timestamps"), TimestampsObject) && TimestampsObject != nullptr)
        {
            if (double TmpServerRecvMs = 0.0; (*TimestampsObject)->TryGetNumberField(TEXT("serverRecvMs"), TmpServerRecvMs))
                NetworkHeader.serverRecvMs = static_cast<int64>(TmpServerRecvMs);
            if (double TmpServerSendMs = 0.0; (*TimestampsObject)->TryGetNumberField(TEXT("serverSendMs"), TmpServerSendMs))
                NetworkHeader.serverSendMs = static_cast<int64>(TmpServerSendMs);
            if (double TmpClientSendMsEcho = 0.0; (*TimestampsObject)->TryGetNumberField(TEXT("clientSendMsEcho"), TmpClientSendMsEcho))
                NetworkHeader.clientSendMsEcho = static_cast<int64>(TmpClientSendMsEcho);
            if (!(*TimestampsObject)->TryGetStringField(TEXT("requestId"), NetworkHeader.requestId))
            {
                (*TimestampsObject)->TryGetStringField(TEXT("requestIdEcho"), NetworkHeader.requestId);
            }
        }

    return NetworkHeader;
}

void JSONParser::ProcessTimeSyncFromHeader(const FString& JsonString, UTimeSyncService* TimeSyncService)
{
	if (!TimeSyncService)
	{
		return;
	}

	ProcessTimeSyncFromPacket(ParsePacket(JsonString), TimeSyncService);
}

void JSONParser::ProcessTimeSyncFromPacket(const FParsedPacket& Packet, UTimeSyncService* TimeSyncService)
{
	if (!TimeSyncService || !Packet.bValid || !Packet.Root.IsValid())
	{
		return;
	}

	FNetworkHeaderStruct NetworkHeader = DeserializeNetworkHeader(Packet.Root);

	// Prefer requestIdEcho (server response); fall back to requestId.
	FString RequestIdToUse;
	if (!NetworkHeader.requestId.IsEmpty())
	{
		RequestIdToUse = NetworkHeader.requestId;
	}

	// Update only when the full server timestamp pair is present.
	if (!RequestIdToUse.IsEmpty() &&
		NetworkHeader.serverRecvMs > 0 &&
		NetworkHeader.serverSendMs > 0)
	{
		bool bUpdated = TimeSyncService->UpdateTimeSyncData(
			RequestIdToUse,
			NetworkHeader.serverRecvMs,
			NetworkHeader.serverSendMs
		);

		if (bUpdated)
		{
			UE_LOG(LogPing, VeryVerbose, TEXT("JSONParser: Updated time sync for request: %s"), *RequestIdToUse);
		}
		else
		{
			UE_LOG(LogPing, VeryVerbose, TEXT("JSONParser: Time sync update failed or filtered for: %s"), *RequestIdToUse);
		}
	}
}

