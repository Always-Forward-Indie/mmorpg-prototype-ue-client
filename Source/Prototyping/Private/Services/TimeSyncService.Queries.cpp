#include "Services/TimeSyncService.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Utils/NetCore/TimeSyncMath.h"

int64 UTimeSyncService::GetCurrentClientTimeMs() const
{
    return GetSystemTimeMs();
}

int64 UTimeSyncService::GetEstimatedServerTimeMs(EServerType ServerType) const
{
    if (const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType))
    {
        // Use filtered offset for more stable estimation
        if (ServerData->SampleCount > 0)
        {
            return GetCurrentClientTimeMs() + static_cast<int64>(FMath::RoundToInt(ServerData->FilteredOffsetMs));
        }
        else
        {
            // Fallback to current sync data if no filtered data available
            return GetCurrentClientTimeMs() + ServerData->CurrentSyncData.TimeOffsetMs;
        }
    }
    
    // Fallback to client time if no sync data
    return GetCurrentClientTimeMs();
}

float UTimeSyncService::GetNetworkLatencyMs(EServerType ServerType) const
{
    if (const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType))
    {
        // Use filtered latency for more stable values
        if (ServerData->SampleCount > 0)
        {
            return ServerData->FilteredLatencyMs;
        }
        else
        {
            return ServerData->CurrentSyncData.NetworkLatencyMs;
        }
    }
    
    return 0.0f;
}

int64 UTimeSyncService::GetTimeOffsetMs(EServerType ServerType) const
{
    if (const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType))
    {
        // Use filtered offset for more stable values
        if (ServerData->SampleCount > 0)
        {
            return static_cast<int64>(FMath::RoundToInt(ServerData->FilteredOffsetMs));
        }
        else
        {
            return ServerData->CurrentSyncData.TimeOffsetMs;
        }
    }
    
    return 0;
}

int64 UTimeSyncService::ClientTimeToServerTime(int64 ClientTimeMs, EServerType ServerType) const
{
    return ClientTimeMs + GetTimeOffsetMs(ServerType);
}

int64 UTimeSyncService::ServerTimeToClientTime(int64 ServerTimeMs, EServerType ServerType) const
{
    return ServerTimeMs - GetTimeOffsetMs(ServerType);
}

float UTimeSyncService::CalculateLagCompensation(int64 ClientActionTimeMs, EServerType ServerType) const
{
    // Calculate how much time has passed on server since the client action
    int64 EstimatedServerActionTime = ClientTimeToServerTime(ClientActionTimeMs, ServerType);
    int64 CurrentServerTime = GetEstimatedServerTimeMs(ServerType);
    
    return static_cast<float>(CurrentServerTime - EstimatedServerActionTime);
}

