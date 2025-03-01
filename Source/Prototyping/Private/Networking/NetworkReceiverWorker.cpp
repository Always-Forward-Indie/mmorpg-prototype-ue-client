// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkReceiverWorker.h"

NetworkReceiverWorker::NetworkReceiverWorker(FSocket* InSocket)
    : Socket(InSocket)
    , bRunThread(true)
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

uint32 NetworkReceiverWorker::Run()
{
    // Ждем, пока соединение не установится
    while (bRunThread && Socket && Socket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
    {
        UE_LOG(LogTemp, Warning, TEXT("Waiting for Receiver socket connection..."));
        FPlatformProcess::Sleep(0.1f); // Ждем 100 мс
    }


    const int32 ReceiveBufferSize = 4096;
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(ReceiveBufferSize);

    TArray<uint8> AccumulatedBuffer; // Буфер для накопления данных

    UE_LOG(LogTemp, Warning, TEXT("NetworkReceiverWorker Thread Started"));

    while (bRunThread)
    {
        int32 BytesRead = 0;
        bool bHasData = Socket->Recv(ReceiveBuffer.GetData(), ReceiveBufferSize, BytesRead);

        if (bHasData && BytesRead > 0)
        {
            // Копируем полученные данные в накопительный буфер
            AccumulatedBuffer.Append(ReceiveBuffer.GetData(), BytesRead);

            int32 DelimiterIndex;
            // Проверяем, есть ли в накопленных данных символ-разделитель '\n'
            while ((DelimiterIndex = AccumulatedBuffer.Find((uint8)'\n')) != INDEX_NONE)
            {
                // Извлекаем один полный пакет
                TArray<uint8> SinglePacket;
                SinglePacket.Append(AccumulatedBuffer.GetData(), DelimiterIndex);

                // Убираем пакет и разделитель из буфера
                AccumulatedBuffer.RemoveAt(0, DelimiterIndex + 1);

                // Преобразуем пакет в FString
                FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(SinglePacket.GetData()), SinglePacket.Num());
                FString ReceivedString(Converter.Length(), Converter.Get());

                UE_LOG(LogTemp, Warning, TEXT("Received packet: %s"), *ReceivedString);

                // Добавляем строку в очередь
                DataQueue.Enqueue(ReceivedString);
            }
        }

        FPlatformProcess::Sleep(0.0001f);
    }

    UE_LOG(LogTemp, Warning, TEXT("NetworkReceiverWorker Thread Exiting"));
    return 0;
}


// Обновлённая функция для преобразования, хотя теперь она может не понадобиться,
// поскольку мы используем FUTF8ToTCHAR непосредственно в Run()
FString NetworkReceiverWorker::StringFromBinaryArray(const uint8* BinaryArray, const int32& ArraySize)
{
    FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(BinaryArray), ArraySize);
    return FString(Converter.Length(), Converter.Get());
}

void NetworkReceiverWorker::Stop()
{
    bRunThread = false;
}

void NetworkReceiverWorker::Exit()
{
}

NetworkReceiverWorker::~NetworkReceiverWorker()
{
}