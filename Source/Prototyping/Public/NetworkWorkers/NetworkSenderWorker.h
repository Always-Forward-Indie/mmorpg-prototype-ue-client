// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

/**
 * 
 */
class PROTOTYPING_API NetworkSenderWorker : public FRunnable
{
	private:
		FSocket* Socket;
		bool bRunThread;
		TQueue<FString, EQueueMode::Mpsc> DataQueue; // Thread-safe queue for messages

	public:
		NetworkSenderWorker(FSocket* InSocket);
		~NetworkSenderWorker();

		// FRunnable interface
		virtual bool Init() override;
		virtual uint32 Run() override;
		virtual void Stop() override;
		virtual void Exit() override;

		void EnqueueDataForSending(const FString& Data);
};
