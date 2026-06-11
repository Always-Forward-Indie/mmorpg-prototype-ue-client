// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkReceiverWorker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Services/TimeSyncService.h"
#include "Prototyping.h"

NetworkReceiverWorker::NetworkReceiverWorker(FSocket* InSocket)
    : Socket(InSocket)
    , bRunThread(true)
    , TimeSyncService(nullptr)
{
}

bool NetworkReceiverWorker::Init()
{
    return true;
}

bool NetworkReceiverWorker::GetData(FString& OutData)
{
    return DataQueue.Dequeue(OutData);
}

void NetworkReceiverWorker::SetTimeSyncService(UTimeSyncService* InTimeSyncService)
{
    TimeSyncService = InTimeSyncService;
}

//FString NetworkReceiverWorker::AddClientReceiveTimestamp(const FString& JsonData, int64 ClientRecvMs)
//{
//    if (!TimeSyncService)
//    {
//        return JsonData;
//    }
//
//    // Parse the JSON to check if it has time sync fields
//    TSharedPtr<FJsonObject> JsonObject;
//    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
//    
//    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
//    {
//        return JsonData; // Return original if parsing fails
//    }
//
//    // Check if header exists and has server timing fields (indicating this is a server response)
//    const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
//    if (!JsonObject->TryGetObjectField(TEXT("header"), HeaderPtr) || !HeaderPtr || !(*HeaderPtr).IsValid())
//    {
//        return JsonData; // No header, return original
//    }
//
//    TSharedPtr<FJsonObject> Header = *HeaderPtr;
//    
//    // Only add clientRecvMs if this is a server response with timing data
//    if (!Header->HasField(TEXT("serverRecvMs")) || !Header->HasField(TEXT("serverSendMs")))
//    {
//        return JsonData; // Not a server response with timing, return original
//    }
//
//    // Add clientRecvMs timestamp (t3)
//    Header->SetNumberField(TEXT("clientRecvMs"), ClientRecvMs);
//
//    // Rebuild the JSON string
//    FString UpdatedJsonString;
//    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
//        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&UpdatedJsonString);
//    
//    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
//    {
//        // Remove any newline characters
//        UpdatedJsonString.ReplaceInline(TEXT("\n"), TEXT(""));
//        UpdatedJsonString.ReplaceInline(TEXT("\r"), TEXT(""));
//        
//        UE_LOG(LogTemp, VeryVerbose, TEXT("NetworkReceiverWorker: Added clientRecvMs %lld to packet: %s"), 
//            ClientRecvMs, *UpdatedJsonString);
//        
//        return UpdatedJsonString;
//    }
//
//    // Fallback to original if serialization fails
//    return JsonData;
//}

FString NetworkReceiverWorker::AddClientReceiveTimestamp(const FString& JsonData, int64 ClientRecvMs)
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

    bool bHasTimingData = false;

    const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("header"), HeaderPtr) && HeaderPtr && (*HeaderPtr).IsValid())
    {
        TSharedPtr<FJsonObject> Header = *HeaderPtr;
        if (Header->HasField(TEXT("serverRecvMs")) && Header->HasField(TEXT("serverSendMs")))
        {
            int64 PreciseClientRecvMs = TimeSyncService.Get()->GetCurrentClientTimeMs();
            Header->SetNumberField(TEXT("clientRecvMs"), PreciseClientRecvMs);
            bHasTimingData = true;
        }
    }

    const TSharedPtr<FJsonObject>* TimestampsPtr = nullptr;
    if (!bHasTimingData && JsonObject->TryGetObjectField(TEXT("timestamps"), TimestampsPtr) && TimestampsPtr && (*TimestampsPtr).IsValid())
    {
        if ((*TimestampsPtr)->HasField(TEXT("serverRecvMs")) && (*TimestampsPtr)->HasField(TEXT("serverSendMs")))
        {
            const TSharedPtr<FJsonObject>* ExistingHeader = nullptr;
            if (!JsonObject->TryGetObjectField(TEXT("header"), ExistingHeader) || !ExistingHeader || !(*ExistingHeader).IsValid())
            {
                JsonObject->SetObjectField(TEXT("header"), MakeShareable(new FJsonObject));
            }
            const TSharedPtr<FJsonObject>* HeaderOut = nullptr;
            JsonObject->TryGetObjectField(TEXT("header"), HeaderOut);
            TSharedPtr<FJsonObject> Header = *HeaderOut;
            int64 PreciseClientRecvMs = TimeSyncService.Get()->GetCurrentClientTimeMs();
            Header->SetNumberField(TEXT("clientRecvMs"), PreciseClientRecvMs);
            bHasTimingData = true;
        }
    }

    if (!bHasTimingData)
    {
        return JsonData;
    }

    FString UpdatedJsonString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&UpdatedJsonString);

    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UpdatedJsonString.ReplaceInline(TEXT("\n"), TEXT(""));
        UpdatedJsonString.ReplaceInline(TEXT("\r"), TEXT(""));
        return UpdatedJsonString;
    }

    return JsonData;
}

uint32 NetworkReceiverWorker::Run()
{
    // ����, ���� ���������� �� �����������
    while (bRunThread && Socket && Socket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
    {
        UE_LOG(LogConnection, Verbose, TEXT("Waiting for Receiver socket connection..."));
        FPlatformProcess::Sleep(0.1f); // ���� 100 ��
    }


    const int32 ReceiveBufferSize = 4096;
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(ReceiveBufferSize);

    TArray<uint8> AccumulatedBuffer; // ����� ��� ���������� ������

    UE_LOG(LogConnection, Log, TEXT("NetworkReceiverWorker Thread Started"));

    while (bRunThread)
    {
        // Guard: DetachSocket() may have been called by NetworkManager::Shutdown()
        // concurrently. If Socket is null we must not call Recv() � exit cleanly.
        FSocket* CurrentSocket = Socket;
        if (!CurrentSocket)
        {
            break;
        }

        int32 BytesRead = 0;
        bool bHasData = CurrentSocket->Recv(ReceiveBuffer.GetData(), ReceiveBufferSize, BytesRead);

        // Re-check both bRunThread and Socket after Recv() unblocks � the socket
        // may have been closed and destroyed by Shutdown() to wake us up.
        if (!bRunThread || !Socket)
        {
            break;
        }

        if (bHasData && BytesRead > 0)
        {
            // Get precise receive timestamp immediately after successful Socket->Recv() (t3)
            //int64 PreciseClientRecvMs = TimeSyncService ? TimeSyncService->GetCurrentClientTimeMs() : 0;
            
            // �������� ���������� ������ � ������������� �����
            AccumulatedBuffer.Append(ReceiveBuffer.GetData(), BytesRead);

            int32 DelimiterIndex;
            // ���������, ���� �� � ����������� ������ ������-����������� '\n'
            while ((DelimiterIndex = AccumulatedBuffer.Find((uint8)'\n')) != INDEX_NONE)
            {
                // ��������� ���� ������ �����
                TArray<uint8> SinglePacket;
                SinglePacket.Append(AccumulatedBuffer.GetData(), DelimiterIndex);

                // ������� ����� � ����������� �� ������
                AccumulatedBuffer.RemoveAt(0, DelimiterIndex + 1);

                // ����������� ����� � FString
                FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(SinglePacket.GetData()), SinglePacket.Num());
                FString ReceivedString(Converter.Length(), Converter.Get());

                // t3 ������ ��������� �����, �� ������ �����:
                const int64 PerPacketT3 = TimeSyncService.IsValid() ? TimeSyncService.Get()->GetCurrentClientTimeMs() : 0;
                // Add clientRecvMs timestamp if this is a server response
                FString TimestampedString = AddClientReceiveTimestamp(ReceivedString, PerPacketT3);

                // ��������� ������ � �������
                DataQueue.Enqueue(TimestampedString);
            }
        }

        FPlatformProcess::Sleep(0.0001f);
    }

    UE_LOG(LogConnection, Log, TEXT("NetworkReceiverWorker Thread Exiting"));
    return 0;
}


// ���������� ������� ��� ��������������, ���� ������ ��� ����� �� ������������,
// ��������� �� ���������� FUTF8ToTCHAR ��������������� � Run()
FString NetworkReceiverWorker::StringFromBinaryArray(const uint8* BinaryArray, const int32& ArraySize)
{
    FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(BinaryArray), ArraySize);
    return FString(Converter.Length(), Converter.Get());
}

void NetworkReceiverWorker::Stop()
{
    bRunThread = false;
}

void NetworkReceiverWorker::DetachSocket()
{
    // Atomically null the socket pointer under the same volatile semantics
    // that the compiler gives to plain pointer stores on x86/x64.
    // This must be called BEFORE ISocketSubsystem::DestroySocket() so that
    // the Run() loop never calls Recv() on a destroyed FSocket object.
    Socket = nullptr;
}

void NetworkReceiverWorker::Exit()
{
}

NetworkReceiverWorker::~NetworkReceiverWorker()
{
}