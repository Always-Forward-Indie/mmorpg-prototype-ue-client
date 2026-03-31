// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Networking/NetworkManager.h"
#include "Gameplay/UI/MonitorStatsWidget.h"
#include "Services/TimeSyncService.h"
#include "PingManager.generated.h"

/**
 * PingManager that uses TimeSyncService for accurate ping measurements
 * and sends periodic ping requests to all servers
 */
UCLASS()
class PROTOTYPING_API UPingManager: public UObject
{
	GENERATED_BODY()
private:
// Network manager reference
UPROPERTY()
UNetworkManager* networkManager;
// World context to get the world time and manage timers
UWorld* worldContext = nullptr;
// Monitor stats widget to update the ping time
UPROPERTY()
UMonitorStatsWidget* MonitorStatsWidget;
// TimeSyncService for accurate ping measurements
UPROPERTY()
UTimeSyncService* TimeSyncService;
	// Timer handle for periodic ping updates
	FTimerHandle PingUpdateTimerHandle;
	// Timer handle for periodic ping requests
	FTimerHandle PingRequestTimerHandle;

public:
	UPingManager(const FObjectInitializer& ObjectInitializer);
	
	// Initialize the ping manager with required dependencies
	void Initialize(UNetworkManager* NetworkManager, UMonitorStatsWidget* MonitorStats);
	
	// Set world context for timer management
	void SetWorldContext(UWorld* World);
	
	// Set TimeSyncService reference
	void SetTimeSyncService(UTimeSyncService* InTimeSyncService);
	
	// Start periodic ping updates to UI and ping requests to servers
	void StartPingUpdates();
	
	// Stop periodic ping updates and requests
	void StopPingUpdates();

	// Re-register ping timers after a level transition.
	// Must be called after SetWorldContext() so timers run in the new world.
	void RestartPingUpdates();
	
	// Get current ping for a specific server type
	UFUNCTION(BlueprintCallable, Category = "Ping")
	float GetServerPing(EServerType ServerType) const;
	
	// Send ping request to all servers
	UFUNCTION(BlueprintCallable, Category = "Ping")
	void SendPingRequestToAllServers();
	
	// Send ping request to specific server
	void SendPingRequestToServer(EServerType ServerType);
	
	// Update UI with current ping values from TimeSyncService
	void UpdatePingDisplay();

	// Legacy method - now deprecated but kept for compatibility
	UE_DEPRECATED(5.0, "Use TimeSyncService directly instead of manual ping calculation")
	void CalculatePingTime(const TArray<FDateTime>& SendTimes, const TArray<FDateTime>& ReceiveTimes, const FString& serverName);

private:
	// Timer callback for updating ping display
	void OnPingUpdateTimer();
	
	// Timer callback for sending ping requests
	void OnPingRequestTimer();
};
