// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// Deserialize network header with time sync data
FNetworkHeaderStruct JSONParser::DeserializeNetworkHeader(const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    FNetworkHeaderStruct NetworkHeader;

    // Convert the string to a JSON object
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
        if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
        {
            if ((*HeaderObject)->HasField(TEXT("eventType")))
            {
                NetworkHeader.eventType = (*HeaderObject)->GetStringField(TEXT("eventType"));
            }

            if ((*HeaderObject)->HasField(TEXT("status")))
            {
                NetworkHeader.status = (*HeaderObject)->GetStringField(TEXT("status"));
            }

            if ((*HeaderObject)->HasField(TEXT("clientId")))
            {
                NetworkHeader.clientId = (*HeaderObject)->GetIntegerField(TEXT("clientId"));
            }

            if ((*HeaderObject)->HasField(TEXT("hash")))
            {
                NetworkHeader.hash = (*HeaderObject)->GetStringField(TEXT("hash"));
            }

            if ((*HeaderObject)->HasField(TEXT("message")))
            {
                NetworkHeader.message = (*HeaderObject)->GetStringField(TEXT("message"));
            }

            // Check for requestIdEcho first (server response), then requestId (client request)
            if ((*HeaderObject)->HasField(TEXT("requestIdEcho")))
            {
                NetworkHeader.requestId = (*HeaderObject)->GetStringField(TEXT("requestIdEcho"));
            }
            else if ((*HeaderObject)->HasField(TEXT("requestId")))
            {
                NetworkHeader.requestId = (*HeaderObject)->GetStringField(TEXT("requestId"));
            }

            // Time sync fields
            if ((*HeaderObject)->HasField(TEXT("clientSendMs")))
            {
                NetworkHeader.clientSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMs")));
            }

            if ((*HeaderObject)->HasField(TEXT("serverRecvMs")))
            {
                NetworkHeader.serverRecvMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("serverRecvMs")));
            }

            if ((*HeaderObject)->HasField(TEXT("serverSendMs")))
            {
                NetworkHeader.serverSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("serverSendMs")));
            }

            if ((*HeaderObject)->HasField(TEXT("clientSendMsEcho")))
            {
                NetworkHeader.clientSendMsEcho = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMsEcho")));
            }
        }

        // Per protocol (�1.4 pongClient), timestamps may be a root-level object
        // (sibling of "header") instead of fields inside "header". Check and override
        // if present so that pongClient responses are parsed correctly.
        const TSharedPtr<FJsonObject>* TimestampsObject = nullptr;
        if (JsonObject->TryGetObjectField(TEXT("timestamps"), TimestampsObject) && TimestampsObject != nullptr)
        {
            if ((*TimestampsObject)->HasField(TEXT("serverRecvMs")))
            {
                NetworkHeader.serverRecvMs = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("serverRecvMs")));
            }
            if ((*TimestampsObject)->HasField(TEXT("serverSendMs")))
            {
                NetworkHeader.serverSendMs = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("serverSendMs")));
            }
            if ((*TimestampsObject)->HasField(TEXT("clientSendMsEcho")))
            {
                NetworkHeader.clientSendMsEcho = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("clientSendMsEcho")));
            }
            if ((*TimestampsObject)->HasField(TEXT("requestId")))
            {
                NetworkHeader.requestId = (*TimestampsObject)->GetStringField(TEXT("requestId"));
            }
            else if ((*TimestampsObject)->HasField(TEXT("requestIdEcho")))
            {
                NetworkHeader.requestId = (*TimestampsObject)->GetStringField(TEXT("requestIdEcho"));
            }
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

	FNetworkHeaderStruct NetworkHeader = DeserializeNetworkHeader(JsonString);

	// ��������� requestIdEcho ��� ������� �������
	FString RequestIdToUse;
	if (!NetworkHeader.requestId.IsEmpty())
	{
		RequestIdToUse = NetworkHeader.requestId; // ����� ���� requestIdEcho �� �������
	}

	// ������������ ������ ���� ���� ��������� ����������
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
			UE_LOG(LogTemp, VeryVerbose, TEXT("JSONParser: Updated time sync for request: %s"), *RequestIdToUse);
		}
		else
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("JSONParser: Time sync update failed or filtered for: %s"), *RequestIdToUse);
		}
	}
}

