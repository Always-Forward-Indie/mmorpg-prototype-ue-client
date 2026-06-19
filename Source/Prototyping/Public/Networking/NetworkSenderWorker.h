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
class PROTOTYPING_API NetworkSenderWorker : public FRunnable
{
	private:
		std::atomic<FSocket*> Socket;
		std::atomic<bool> bRunThread;
		TQueue<FString, EQueueMode::Mpsc> DataQueue; // Thread-safe queue for messages
		TWeakObjectPtr<UTimeSyncService> TimeSyncService; // Weak ref - GC-safe access from worker thread

	public:
		NetworkSenderWorker(FSocket* InSocket);
		~NetworkSenderWorker();

		// FRunnable interface
		virtual bool Init() override;
		virtual uint32 Run() override;
		virtual void Stop() override;
		virtual void Exit() override;

		void EnqueueDataForSending(const FString& Data);

		// Atomically clear the socket pointer so Run() stops calling Send()
		// on the socket object. Must be called BEFORE ISocketSubsystem::DestroySocket().
		void DetachSocket();

		// Set TimeSyncService reference for precise timestamp updates
		void SetTimeSyncService(UTimeSyncService* InTimeSyncService);

	private:
		// Update clientSendMs timestamp right before sending for maximum accuracy
		FString UpdateClientSendTimestamp(const FString& JsonData);
};
