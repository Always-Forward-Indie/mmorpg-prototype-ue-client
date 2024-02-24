// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

/**
 * 
 */
class PROTOTYPING_API NetworkReceiverWorker : public FRunnable
{
private:
	FSocket* Socket; // Socket to receive data on
    bool bRunThread; // Flag to control thread execution
    TQueue<FString, EQueueMode::Mpsc> DataQueue; // Thread-safe queue for messages

public:
    // Constructor
	NetworkReceiverWorker(FSocket* InSocket);

    FString StringFromBinaryArray(const uint8* BinaryArray, const int32& ArraySize);

    // Method to safely retrieve data from the queue
    bool GetData(FString& OutData);

    // FRunnable interface implementation
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    // Destructor
	~NetworkReceiverWorker();
};
