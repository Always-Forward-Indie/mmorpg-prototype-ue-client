// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkWorkers/NetworkSenderWorker.h"

NetworkSenderWorker::NetworkSenderWorker(FSocket* InSocket) : Socket(InSocket), bRunThread(true)
{
}

bool NetworkSenderWorker::Init()
{
	return true;
}

void NetworkSenderWorker::EnqueueDataForSending(const FString& Data)
{
	FString DelimitedData = Data + TEXT("\r\n\r\n"); // Using newline character as delimiter
	DataQueue.Enqueue(DelimitedData);
}

uint32 NetworkSenderWorker::Run()
{
	while (bRunThread)
	{
		FString Data;
		if (DataQueue.Dequeue(Data))
		{
			// Convert the string to a byte array
			TArray<uint8> SendBuffer;
			FTCHARToUTF8 ConvertedData(*Data);
			SendBuffer.Append((uint8*)ConvertedData.Get(), ConvertedData.Length());

			// Send the data
			int32 BytesSent = 0;
			bool bSuccessful = Socket->Send(SendBuffer.GetData(), SendBuffer.Num(), BytesSent);

			if (!bSuccessful)
			{
				// An error occurred
			}
		}

		// Sleep for a short duration if no data to prevent a tight loop that hogs the CPU
		FPlatformProcess::Sleep(0.01);
	}

	return 0;
}

void NetworkSenderWorker::Stop()
{
	bRunThread = false;
}

void NetworkSenderWorker::Exit()
{
	bRunThread = false;
}

NetworkSenderWorker::~NetworkSenderWorker()
{
}
