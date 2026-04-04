// Fill out your copyright notice in the Description page of Project Settings.

#include "Networking/NetworkSenderWorker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Services/TimeSyncService.h"
#include "Prototyping.h"

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
    if (!TimeSyncService.IsValid())
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

    // Per protocol: timestamps are a sub-object inside header containing
    // clientSendMsEcho and requestId. Update clientSendMsEcho with precise time.
    int64 PreciseClientSendMs = 0;
    const TSharedPtr<FJsonObject>* TimestampsPtr = nullptr;
    if (Header->TryGetObjectField(TEXT("timestamps"), TimestampsPtr) && TimestampsPtr && (*TimestampsPtr).IsValid())
    {
        PreciseClientSendMs = TimeSyncService.Get()->GetCurrentClientTimeMs();
        (*TimestampsPtr)->SetNumberField(TEXT("clientSendMsEcho"), PreciseClientSendMs);
    }
    else
    {
        // Fallback: check for legacy flat fields
        bool bShouldUpdateTimestamp = Header->HasField(TEXT("requestId")) || Header->HasField(TEXT("clientSendMs"));
        if (!bShouldUpdateTimestamp)
        {
            return JsonData;
        }
        PreciseClientSendMs = TimeSyncService.Get()->GetCurrentClientTimeMs();
        Header->SetNumberField(TEXT("clientSendMs"), PreciseClientSendMs);
    }

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
        UE_LOG(LogConnection, Verbose, TEXT("Waiting for Sender socket connection..."));
        FPlatformProcess::Sleep(0.1f);
    }

    while (bRunThread)
    {
        // Guard: DetachSocket() may have been called concurrently by Shutdown().
        FSocket* CurrentSocket = Socket;
        if (!CurrentSocket)
        {
            break;
        }

        // Socket may be nulled or closed by NetworkManager::Shutdown().
        // Check both flag and socket state before every send attempt.
        if (CurrentSocket->GetConnectionState() == ESocketConnectionState::SCS_NotConnected)
        {
            UE_LOG(LogConnection, Warning, TEXT("SenderWorker: socket lost, exiting."));
            break;
        }

        FString Data;
        if (DataQueue.Dequeue(Data))
        {
            // 1) обновляем таймштамп
            FString UpdatedData = UpdateClientSendTimestamp(Data);

            // 2) гарантируем ровно один \n как разделитель
            if (!UpdatedData.EndsWith(TEXT("\n")))
            {
                UpdatedData.AppendChar(TEXT('\n'));
            }

            // 3) Guard: re-check bRunThread and socket before the blocking Send()
            if (!bRunThread || !Socket)
            {
                break;
            }

            FTCHARToUTF8 ConvertedData(*UpdatedData);
            TArray<uint8> SendBuffer(reinterpret_cast<const uint8*>(ConvertedData.Get()), ConvertedData.Length());

            int32 BytesSent = 0;
            bool bSuccessful = CurrentSocket->Send(SendBuffer.GetData(), SendBuffer.Num(), BytesSent);

            if (!bSuccessful || BytesSent == 0)
            {
                UE_LOG(LogNetPacket, Warning, TEXT("Failed to send data."));
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

void NetworkSenderWorker::DetachSocket()
{
    // Atomically null the socket pointer. Must be called BEFORE
    // ISocketSubsystem::DestroySocket() so Run() never calls Send()
    // on a destroyed FSocket object.
    Socket = nullptr;
}

void NetworkSenderWorker::Exit()
{
    Stop();
}


NetworkSenderWorker::~NetworkSenderWorker()
{
	// Stop the thread
	Stop();
	// Note: socket lifetime is managed by NetworkManager, not here
}


