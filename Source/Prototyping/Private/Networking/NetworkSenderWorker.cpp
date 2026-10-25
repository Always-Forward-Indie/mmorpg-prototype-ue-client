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
    // v2 framing is applied in Run() after UTF-8 conversion (the length
    // prefix counts UTF-8 bytes). Queued payload is the raw JSON.
    DataQueue.Enqueue(Data);
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
        UE_LOG(LogPing, VeryVerbose, TEXT("NetworkSenderWorker: Updated clientSendMs to %lld"), PreciseClientSendMs);
        return UpdatedJsonString;
    }

    return JsonData;
}

uint32 NetworkSenderWorker::Run()
{
    while (bRunThread)
    {
        FSocket* Sock = Socket.load();
        if (!Sock || Sock->GetConnectionState() == ESocketConnectionState::SCS_Connected)
        {
            break;
        }
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

            // 2) v2 framing: [u32 LE byte-length][UTF-8 JSON], no delimiter.
            // Length counts UTF-8 bytes (multi-byte codepoints stay whole —
            // the receiver decodes only complete frames).
            FTCHARToUTF8 ConvertedData(*UpdatedData);
            const int32 Utf8Len = ConvertedData.Length();
            TArray<uint8> SendBuffer;
            SendBuffer.SetNumUninitialized(4 + Utf8Len);
            const uint32 LenPrefix = static_cast<uint32>(Utf8Len);
            SendBuffer[0] = static_cast<uint8>(LenPrefix & 0xFF);
            SendBuffer[1] = static_cast<uint8>((LenPrefix >> 8) & 0xFF);
            SendBuffer[2] = static_cast<uint8>((LenPrefix >> 16) & 0xFF);
            SendBuffer[3] = static_cast<uint8>((LenPrefix >> 24) & 0xFF);
            if (Utf8Len > 0)
            {
                FMemory::Memcpy(SendBuffer.GetData() + 4, ConvertedData.Get(), Utf8Len);
            }

            // 3) Guard: re-check bRunThread and socket before the blocking Send()
            if (!bRunThread || !Socket)
            {
                break;
            }

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


