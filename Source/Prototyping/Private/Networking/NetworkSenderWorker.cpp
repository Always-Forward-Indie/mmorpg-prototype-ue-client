// Fill out your copyright notice in the Description page of Project Settings.

#include "Networking/NetworkSenderWorker.h"

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
	FString DelimitedData = Data + TEXT("\r\n\r\n"); // Using "\r\n\r\n" as data packet delimiter
    DataQueue.Enqueue(DelimitedData);
}

uint32 NetworkSenderWorker::Run()
{
    // Ждем, пока соединение не установится
    while (bRunThread && Socket && Socket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
    {
        UE_LOG(LogTemp, Warning, TEXT("Waiting for Sender socket connection..."));
        FPlatformProcess::Sleep(0.1f); // Ждем 100 мс
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
            // Конвертируем FString в байтовый массив
            FTCHARToUTF8 ConvertedData(*Data);
            TArray<uint8> SendBuffer((uint8*)ConvertedData.Get(), ConvertedData.Length());

            int32 BytesSent = 0;
            bool bSuccessful = Socket->Send(SendBuffer.GetData(), SendBuffer.Num(), BytesSent);

			UE_LOG(LogTemp, Warning, TEXT("Sent to server %d bytes of data."), BytesSent);
			UE_LOG(LogTemp, Warning, TEXT("Data sent: %s"), *Data);

            if (!bSuccessful || BytesSent == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to send data."));
            }
        }
        else
        {
            // Даем потоку немного отдохнуть, чтобы не загружать CPU
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
