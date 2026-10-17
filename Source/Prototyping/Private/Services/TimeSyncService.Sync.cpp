#include "Services/TimeSyncService.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Utils/NetCore/TimeSyncMath.h"

FString UTimeSyncService::GenerateAndRegisterSyncRequest(EServerType ServerType, int64 InClientSendMs)
{
    // Use the provided timestamp (the actual send moment) so that t0 stored in
    // PendingRequests matches the value embedded in the outgoing JSON header.
    // Fallback to now() when not provided (e.g. callers that don't need precision).
    int64 CurrentTime = (InClientSendMs > 0) ? InClientSendMs : GetCurrentClientTimeMs();
    
    // Ensure server sync data exists
    EnsureServerSyncDataExists(ServerType);
    
    // Generate unique request ID without any interval restrictions
    FString RequestId = GenerateUniqueRequestId();
    
    // Register pending request
    int64 TimeoutTime = CurrentTime + RequestTimeoutMs;
    PendingRequests.Add(FPendingSyncRequest(RequestId, ServerType, CurrentTime, TimeoutTime));
    
    UE_LOG(LogTemp, Verbose, TEXT("TimeSyncService: Generated request ID for %s: %s at %lld"), 
        *GetServerTypeName(ServerType), *RequestId, CurrentTime);
    
    return RequestId;
}

bool UTimeSyncService::UpdateTimeSyncData(const FString& RequestId, int64 ServerRecvMs, int64 ServerSendMs)
{
    // Find pending request
    FPendingSyncRequest* PendingRequest = FindPendingRequest(RequestId);
    if (!PendingRequest)
    {
        UE_LOG(LogTemp, Verbose, TEXT("TimeSyncService: Received response for unknown or expired request ID: %s"), *RequestId);
        // Don't return false - this is normal for regular game requests that don't need sync tracking
        return true;
    }

    // Get current time for client receive timestamp
    int64 ClientRecvMs = GetCurrentClientTimeMs();
    
    // Create new sync data with NTP-style timestamps
    FTimeSyncData NewSyncData;
    NewSyncData.ClientSendMs = PendingRequest->ClientSendMs;  // t0
    NewSyncData.ServerRecvMs = ServerRecvMs;                  // t1
    NewSyncData.ServerSendMs = ServerSendMs;                  // t2
    NewSyncData.ClientRecvMs = ClientRecvMs;                  // t3
    NewSyncData.ServerType = PendingRequest->ServerType;

    // Calculate RTT and sample quality
    NewSyncData.RoundTripTimeMs = static_cast<float>(NewSyncData.ClientRecvMs - NewSyncData.ClientSendMs);
    NewSyncData.SampleQuality = CalculateSampleQuality(NewSyncData);

    // Validate sample quality
    if (!IsSampleValid(NewSyncData))
    {
        UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Discarding invalid sample for %s - RTT: %.1fms, Quality: %.1f"),
            *GetServerTypeName(PendingRequest->ServerType), NewSyncData.RoundTripTimeMs, NewSyncData.SampleQuality);
        RemovePendingRequest(RequestId);
        return false;
    }

    // Calculate NTP-style offset and latency with high precision
    NewSyncData.TimeOffsetMs = CalculateNTPOffset(NewSyncData);
    NewSyncData.NetworkLatencyMs = CalculateNTPLatency(NewSyncData);

    // Check if this is a "near-best" sample worth using for filtering
    if (!IsNearBestSample(PendingRequest->ServerType, NewSyncData))
    {
        UE_LOG(LogTemp, Verbose, TEXT("TimeSyncService: Discarding poor quality sample for %s - RTT: %.1fms not near-best"),
            *GetServerTypeName(PendingRequest->ServerType), NewSyncData.RoundTripTimeMs);
        RemovePendingRequest(RequestId);
        return true; // Return true as this is not an error, just filtering
    }

    // Add to history
    AddSyncDataToHistory(PendingRequest->ServerType, NewSyncData);

    // Apply EWMA filtering with step limiting
    ApplyEWMAFiltering(PendingRequest->ServerType, NewSyncData);

    // Update current sync data with filtered values
    UpdateAveragedSyncData(PendingRequest->ServerType);

    int64 ServerProcessingTimeMs = NewSyncData.ServerSendMs - NewSyncData.ServerRecvMs;
    UE_LOG(LogTemp, Log, TEXT("TimeSyncService: Updated %s - RTT: %.1fms, Latency: %.1fms, Offset: %lldms, ServerProcessing: %lldms, Quality: %.1f"),
        *GetServerTypeName(PendingRequest->ServerType), NewSyncData.RoundTripTimeMs, NewSyncData.NetworkLatencyMs, 
        NewSyncData.TimeOffsetMs, ServerProcessingTimeMs, NewSyncData.SampleQuality);

    // Remove the pending request
    RemovePendingRequest(RequestId);
    
    return true;
}

bool UTimeSyncService::IsTimeSyncValid(EServerType ServerType) const
{
    const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData || ServerData->SyncHistory.Num() == 0)
    {
        return false;
    }

    int64 CurrentTime = GetCurrentClientTimeMs();
    int64 LastSyncTime = ServerData->SyncHistory.Last().ClientRecvMs;
    
    return (CurrentTime - LastSyncTime) < MaxSyncAgeMs;
}

void UTimeSyncService::RequestTimeSync(EServerType ServerType)
{
    FString RequestId = GenerateAndRegisterSyncRequest(ServerType);
    if (RequestId.IsEmpty())
    {
        return; // Too soon since last request
    }
    
    // TODO: Send time sync request to server with RequestId
    // This would be implemented by the networking system
    UE_LOG(LogTemp, Log, TEXT("TimeSyncService: Requesting time sync for %s with ID: %s"), 
        *GetServerTypeName(ServerType), *RequestId);
}

FTimeSyncData UTimeSyncService::GetCurrentTimeSyncData(EServerType ServerType) const
{
    if (const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType))
    {
        return ServerData->CurrentSyncData;
    }
    
    // Return default sync data with correct server type
    FTimeSyncData DefaultData;
    DefaultData.ServerType = ServerType;
    return DefaultData;
}

void UTimeSyncService::LogTimeSyncStats(EServerType ServerType) const
{
    const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData)
    {
        UE_LOG(LogTemp, Warning, TEXT("=== %s Time Sync Statistics ==="), *GetServerTypeName(ServerType));
        UE_LOG(LogTemp, Warning, TEXT("No sync data available"));
        UE_LOG(LogTemp, Warning, TEXT("============================"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== %s Time Sync Statistics ==="), *GetServerTypeName(ServerType));
    UE_LOG(LogTemp, Warning, TEXT("Samples: %d"), ServerData->SyncHistory.Num());
    
    if (ServerData->SampleCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Filtered Latency: %.2fms"), ServerData->FilteredLatencyMs);
        UE_LOG(LogTemp, Warning, TEXT("Filtered Offset: %.2fms"), ServerData->FilteredOffsetMs);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Current Latency: %.2fms"), ServerData->CurrentSyncData.NetworkLatencyMs);
    UE_LOG(LogTemp, Warning, TEXT("Current Offset: %lldms"), ServerData->CurrentSyncData.TimeOffsetMs);
    UE_LOG(LogTemp, Warning, TEXT("Sync Valid: %s"), IsTimeSyncValid(ServerType) ? TEXT("Yes") : TEXT("No"));
    
    if (ServerData->SyncHistory.Num() > 0)
    {
        float AvgLatency = CalculateAverageLatency(ServerType);
        int64 AvgOffset = CalculateAverageTimeOffset(ServerType);
        const FTimeSyncData& Latest = ServerData->SyncHistory.Last();
        
        // Calculate best RTT in recent history
        float BestRTT = Latest.RoundTripTimeMs;
        int32 SamplesToCheck = FMath::Min(ServerData->SyncHistory.Num(), 10);
        for (int32 i = ServerData->SyncHistory.Num() - SamplesToCheck; i < ServerData->SyncHistory.Num(); ++i)
        {
            if (i >= 0)
            {
                BestRTT = FMath::Min(BestRTT, ServerData->SyncHistory[i].RoundTripTimeMs);
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Average Latency: %.2fms"), AvgLatency);
        UE_LOG(LogTemp, Warning, TEXT("Average Offset: %lldms"), AvgOffset);
        UE_LOG(LogTemp, Warning, TEXT("Last RTT: %.2fms"), Latest.RoundTripTimeMs);
        UE_LOG(LogTemp, Warning, TEXT("Best RTT (last 10): %.2fms"), BestRTT);
        UE_LOG(LogTemp, Warning, TEXT("Last Quality: %.2f"), Latest.SampleQuality);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Current Client Time: %lld"), GetCurrentClientTimeMs());
    UE_LOG(LogTemp, Warning, TEXT("Estimated Server Time: %lld"), GetEstimatedServerTimeMs(ServerType));
    UE_LOG(LogTemp, Warning, TEXT("EWMA Smoothing Factor: %.3f"), FMath::Clamp(EWMASmoothingFactor, 0.01f, 0.5f));
    UE_LOG(LogTemp, Warning, TEXT("Max Offset Step: %.1fms"), MaxOffsetStepMs);
    UE_LOG(LogTemp, Warning, TEXT("Max Latency Step: %.1fms"), MaxLatencyStepMs);
    UE_LOG(LogTemp, Warning, TEXT("============================"));
}

void UTimeSyncService::LogAllServersTimeSyncStats() const
{
    LogTimeSyncStats(EServerType::LoginServer);
    LogTimeSyncStats(EServerType::GameServer);
    LogTimeSyncStats(EServerType::ChunkServer);
    
    UE_LOG(LogTemp, Warning, TEXT("=== Pending Requests ==="));
    UE_LOG(LogTemp, Warning, TEXT("Count: %d"), PendingRequests.Num());
    for (const FPendingSyncRequest& Request : PendingRequests)
    {
        UE_LOG(LogTemp, Warning, TEXT("  ID: %s, Server: %s, Age: %lldms"), 
            *Request.RequestId, *GetServerTypeName(Request.ServerType), 
            GetCurrentClientTimeMs() - Request.ClientSendMs);
    }
    UE_LOG(LogTemp, Warning, TEXT("========================"));
}

void UTimeSyncService::CleanupExpiredRequests()
{
    int64 CurrentTime = GetCurrentClientTimeMs();
    
    for (int32 i = PendingRequests.Num() - 1; i >= 0; --i)
    {
        if (CurrentTime > PendingRequests[i].TimeoutMs)
        {
            UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Request %s to %s expired"), 
                *PendingRequests[i].RequestId, *GetServerTypeName(PendingRequests[i].ServerType));
            PendingRequests.RemoveAt(i);
        }
    }
}

FString UTimeSyncService::GetServerTypeName(EServerType ServerType) const
{
    switch (ServerType)
    {
        case EServerType::LoginServer: return TEXT("LoginServer");
        case EServerType::GameServer: return TEXT("GameServer");
        case EServerType::ChunkServer: return TEXT("ChunkServer");
        default: return TEXT("Unknown");
    }
}

