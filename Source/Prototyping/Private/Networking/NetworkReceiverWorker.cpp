// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkReceiverWorker.h"

NetworkReceiverWorker::NetworkReceiverWorker(FSocket* InSocket) : Socket(InSocket), bRunThread(true)
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
    // Assuming you've defined these somewhere
    const int32 ReceiveBufferSize = 4096;
    uint8 ReceiveBuffer[ReceiveBufferSize];

    UE_LOG(LogTemp, Warning, TEXT("NetworkReceiverWorker Thread Started"));

    while (bRunThread)
    {
        int32 BytesRead = 0;
        bool bHasData = Socket->Recv(ReceiveBuffer, ReceiveBufferSize, BytesRead);

        if (bHasData && BytesRead > 0)
        {
            // Process the received data - for example, convert it to a string
            FString ReceivedString = StringFromBinaryArray(ReceiveBuffer, BytesRead);

           // UE_LOG(LogTemp, Warning, TEXT("Received Data: %s"), *ReceivedString);

            // Enqueue the received data for processing on the game thread
            DataQueue.Enqueue(ReceivedString);
        }
        else if (BytesRead == 0)
        {
            // No data received - this is OK, just means there's no data waiting to be processed
        }
        else
        {
            // An error occurred - you may want to handle this and potentially set bRunThread to false
        }

        // Sleep for a short duration if no data to prevent a tight loop that hogs the CPU
        FPlatformProcess::Sleep(0.01);
    }

    UE_LOG(LogTemp, Warning, TEXT("NetworkReceiverWorker Thread Exiting"));

    return 0;
}

// Helper function to convert a byte array to a string
FString NetworkReceiverWorker::StringFromBinaryArray(const uint8* BinaryArray, const int32& ArraySize)
{
    // Make sure the array is null-terminated
    uint8* NullTerminatedArray = new uint8[ArraySize + 1];
    FMemory::Memcpy(NullTerminatedArray, BinaryArray, ArraySize);
    NullTerminatedArray[ArraySize] = 0; // Null-terminate the array

    // Assuming the binary array is ASCII-encoded
    FString StringData = FString(UTF8_TO_TCHAR(NullTerminatedArray));

    // Clean up
    delete[] NullTerminatedArray;

    return StringData;
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
