// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include <atomic>

// Forward declaration
class UTimeSyncService;

/**
 * 
 */
class PROTOTYPING_API NetworkReceiverWorker : public FRunnable
{
private:
	std::atomic<FSocket*> Socket; // Socket to receive data on
	std::atomic<bool> bRunThread; // Flag to control thread execution
	TQueue<FString, EQueueMode::Mpsc> DataQueue; // Thread-safe queue for messages
	TWeakObjectPtr<UTimeSyncService> TimeSyncService; // Weak ref - GC-safe access from worker thread

public:
    // Constructor
	NetworkReceiverWorker(FSocket* InSocket);

    FString StringFromBinaryArray(const uint8* BinaryArray, const int32& ArraySize);

    // Method to safely retrieve data from the queue
    bool GetData(FString& OutData);

    // Set TimeSyncService reference for precise timestamp updates
    void SetTimeSyncService(UTimeSyncService* InTimeSyncService);

    // Atomically clear the socket pointer so Run() stops calling Recv()
    // on the socket object. Must be called BEFORE ISocketSubsystem::DestroySocket()
    // to prevent a vtable-read crash on the receiver thread.
    void DetachSocket();

    // FRunnable interface implementation
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    // Destructor
	~NetworkReceiverWorker();

private:
    // Add clientRecvMs timestamp right after receiving for maximum accuracy
    FString AddClientReceiveTimestamp(const FString& JsonData, int64 ClientRecvMs);
};
