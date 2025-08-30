// Fill out your copyright notice in the Description page of Project Settings.

#include "Networking/NetworkSenderWorker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Services/TimeSyncService.h"

NetworkSenderWorker::NetworkSenderWorker(FSocket* InSocket)
    : Socket(InSocket), bRunThread(true)
{
}

bool NetworkSenderWorker::Init()
{
    return true;
}

void NetworkSenderWorker::EnqueueDataForSending(const FString& Data)
{
	FString DelimitedData = Data + TEXT("\n"); // Using "\n" as data packet delimiter
    DataQueue.Enqueue(DelimitedData);
}

void NetworkSenderWorker::SetTimeSyncService(UTimeSyncService* InTimeSyncService)
{
    TimeSyncService = InTimeSyncService;
}

FString NetworkSenderWorker::UpdateClientSendTimestamp(const FString& JsonData)
{
    if (!TimeSyncService)
    {
        return JsonData;
    }

    // Parse the JSON to check if it has time sync fields
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return JsonData; // Return original if parsing fails
    }

    // Check if header exists
    const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("header"), HeaderPtr) || !HeaderPtr || !(*HeaderPtr).IsValid())
    {
        return JsonData; // No header, return original
    }

    TSharedPtr<FJsonObject> Header = *HeaderPtr;
    
    // Update timestamp for ping requests (have requestId) or any packet with clientSendMs field
    bool bShouldUpdateTimestamp = Header->HasField(TEXT("requestId")) || Header->HasField(TEXT("clientSendMs"));
    
    if (!bShouldUpdateTimestamp)
    {
        return JsonData; // Not a sync/ping request, return original
    }

    // Update clientSendMs with current precise timestamp (t0)
    int64 PreciseClientSendMs = TimeSyncService->GetCurrentClientTimeMs();
    Header->SetNumberField(TEXT("clientSendMs"), PreciseClientSendMs);

    // Rebuild the JSON string
    FString UpdatedJsonString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&UpdatedJsonString);
    
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("NetworkSenderWorker: Updated clientSendMs to %lld for packet: %s"), 
            PreciseClientSendMs, *UpdatedJsonString);
        
        return UpdatedJsonString;
    }

    // Fallback to original if serialization fails
    return JsonData;
}

uint32 NetworkSenderWorker::Run()
{
    while (bRunThread && Socket && Socket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
    {
        UE_LOG(LogTemp, Warning, TEXT("Waiting for Sender socket connection..."));
        FPlatformProcess::Sleep(0.1f);
    }

    while (bRunThread)
    {
        if (!Socket || Socket->GetConnectionState() == ESocketConnectionState::SCS_NotConnected)
        {
            UE_LOG(LogTemp, Warning, TEXT("Socket is not connected."));
            break;
        }

        FString Data;
        if (DataQueue.Dequeue(Data))
        {
            // 1) обновл€ем таймштамп
            FString UpdatedData = UpdateClientSendTimestamp(Data);

            // 2) гарантируем ровно один \n как разделитель
            if (!UpdatedData.EndsWith(TEXT("\n")))
            {
                UpdatedData.AppendChar(TEXT('\n'));
            }

            // 3) кодируем и отправл€ем (убери дублирующее объ€вление!)
            FTCHARToUTF8 ConvertedData(*UpdatedData);
            TArray<uint8> SendBuffer(reinterpret_cast<const uint8*>(ConvertedData.Get()), ConvertedData.Length());

            int32 BytesSent = 0;
            bool bSuccessful = Socket->Send(SendBuffer.GetData(), SendBuffer.Num(), BytesSent);

            UE_LOG(LogTemp, Warning, TEXT("Sent to server %d bytes of data."), BytesSent);
            UE_LOG(LogTemp, Warning, TEXT("Data sent: %s"), *UpdatedData);

            if (!bSuccessful || BytesSent == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to send data."));
            }
        }
        else
        {
            FPlatformProcess::Sleep(0.01f);
        }
    }
    return 0;
}

void NetworkSenderWorker::Stop()
{
    bRunThread = false;
}

void NetworkSenderWorker::Exit()
{
    Stop();
}

NetworkSenderWorker::~NetworkSenderWorker()
{
	// Stop the thread
	if (!bRunThread)
	{
        Stop();
	}

	// Clean up the socket
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
    }
}
