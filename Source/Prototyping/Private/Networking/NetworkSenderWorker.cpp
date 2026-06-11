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

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return JsonData;
    }

    const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("header"), HeaderPtr) || !HeaderPtr || !(*HeaderPtr).IsValid())
    {
        return JsonData;
    }

    TSharedPtr<FJsonObject> Header = *HeaderPtr;

    if (!Header->HasField(TEXT("requestId")) && !Header->HasField(TEXT("clientSendMs")))
    {
        return JsonData;
    }

    int64 PreciseClientSendMs = TimeSyncService.Get()->GetCurrentClientTimeMs();
    Header->SetNumberField(TEXT("clientSendMs"), PreciseClientSendMs);

    FString UpdatedJsonString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&UpdatedJsonString);
    
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("NetworkSenderWorker: Updated clientSendMs to %lld"), PreciseClientSendMs);
        return UpdatedJsonString;
    }

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
            // 1) ��������� ���������
            FString UpdatedData = UpdateClientSendTimestamp(Data);

            // 2) ����������� ����� ���� \n ��� �����������
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


