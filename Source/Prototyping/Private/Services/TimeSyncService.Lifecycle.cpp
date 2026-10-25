#include "Services/TimeSyncService.h"
#include "Prototyping.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Utils/NetCore/TimeSyncMath.h"


UTimeSyncService::UTimeSyncService()
{
    // Initialize server sync data for all server types
    ServerSyncData.Empty();
    PendingRequests.Empty();
    RequestIdCounter = 0;
    WorldContext = nullptr; // Initialize world context
}

void UTimeSyncService::Initialize()
{
    UE_LOG(LogPing, Verbose, TEXT("TimeSyncService: Initialized"));
    
    // Initialize sync data for all server types
    EnsureServerSyncDataExists(EServerType::LoginServer);
    EnsureServerSyncDataExists(EServerType::GameServer);
    EnsureServerSyncDataExists(EServerType::ChunkServer);
    
    // Initialize current time for all servers
    int64 CurrentTime = GetCurrentClientTimeMs();
    for (auto& ServerData : ServerSyncData)
    {
        ServerData.Value.CurrentSyncData.ClientSendMs = CurrentTime;
        ServerData.Value.CurrentSyncData.ServerType = ServerData.Key;
    }

    // Schedule periodic cleanup of expired requests using proper world context
    if (WorldContext)
    {
        FTimerManager& TimerManager = WorldContext->GetTimerManager();
        TimerManager.SetTimer(CleanupTimerHandle, this, &UTimeSyncService::CleanupExpiredRequests, 30.0f, true);
        UE_LOG(LogPing, Verbose, TEXT("TimeSyncService: Cleanup timer started"));
    }
    else
    {
        UE_LOG(LogPing, Error, TEXT("TimeSyncService: WorldContext is null, cannot start cleanup timer"));
    }
}

void UTimeSyncService::SetWorldContext(UWorld* World)
{
    WorldContext = World;
    
    // Invalidate the stale handle from the previous world before checking IsValid().
    // After a level transition the old TimerManager is destroyed so the handle
    // may report IsValid()==true but actually point into freed memory.
    CleanupTimerHandle.Invalidate();

    // Re-register the cleanup timer in the new world's TimerManager
    if (WorldContext)
    {
        FTimerManager& TimerManager = WorldContext->GetTimerManager();
        TimerManager.SetTimer(CleanupTimerHandle, this, &UTimeSyncService::CleanupExpiredRequests, 30.0f, true);
        UE_LOG(LogPing, Verbose, TEXT("TimeSyncService: Cleanup timer started with new world context"));
    }
}

