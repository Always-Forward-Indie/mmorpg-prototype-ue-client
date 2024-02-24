// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Networking/NetworkManager.h"
#include "Gameplay/UI/MonitorStatsWidget.h"
#include "PingManager.generated.h"


/**
 * 
 */
UCLASS()
class PROTOTYPING_API UPingManager: public UObject
{
	GENERATED_BODY()
private:
	// Network manager to send and receive data
	UNetworkManager* networkManager;
	// World context to get the world time
	UWorld* worldContext;
	// Monitor stats widget to update the ping time
	UMonitorStatsWidget* MonitorStatsWidget;

public:
	UPingManager(const FObjectInitializer& ObjectInitializer);
	void Initialize(UNetworkManager* NetworkManager, UMonitorStatsWidget* MonitorStats);
	void SetWorldContext(UWorld* World);
	void CalculatePingTime(const TArray<FDateTime>& SendTimes, const TArray<FDateTime>& ReceiveTimes, const FString& serverName);
};
