#include "Services/TimeSyncService.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"

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
    UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Initialized"));
    
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
        UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Cleanup timer started"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TimeSyncService: WorldContext is null, cannot start cleanup timer"));
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
        UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Cleanup timer started with new world context"));
    }
}

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

float UTimeSyncService::CalculateAverageLatency(EServerType ServerType) const
{
    const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData || ServerData->SyncHistory.Num() == 0)
    {
        return 0.0f;
    }

    float TotalLatency = 0.0f;
    for (const FTimeSyncData& Sample : ServerData->SyncHistory)
    {
        TotalLatency += Sample.NetworkLatencyMs;
    }

    return TotalLatency / static_cast<float>(ServerData->SyncHistory.Num());
}

int64 UTimeSyncService::CalculateAverageTimeOffset(EServerType ServerType) const
{
    const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData || ServerData->SyncHistory.Num() == 0)
    {
        return 0;
    }

    int64 TotalOffset = 0;
    for (const FTimeSyncData& Sample : ServerData->SyncHistory)
    {
        TotalOffset += Sample.TimeOffsetMs;
    }

    return TotalOffset / ServerData->SyncHistory.Num();
}

void UTimeSyncService::AddSyncDataToHistory(EServerType ServerType, const FTimeSyncData& SyncData)
{
    EnsureServerSyncDataExists(ServerType);
    FServerTimeSyncData& ServerData = ServerSyncData[ServerType];
    
    ServerData.SyncHistory.Add(SyncData);

    // Maintain maximum number of samples
    if (ServerData.SyncHistory.Num() > MaxSyncSamples)
    {
        ServerData.SyncHistory.RemoveAt(0);
    }
}

int64 UTimeSyncService::CalculateNTPOffset(const FTimeSyncData& SyncData) const
{
    // NTP offset calculation with higher precision:
    // offset = ((t1 - t0) + (t2 - t3)) / 2
    // Use float calculation to avoid systematic rounding errors, then round at the end
    double ClientToServer = static_cast<double>(SyncData.ServerRecvMs - SyncData.ClientSendMs);
    double ServerToClient = static_cast<double>(SyncData.ServerSendMs - SyncData.ClientRecvMs);
    double PreciseOffset = (ClientToServer + ServerToClient) / 2.0;
    
    return static_cast<int64>(FMath::RoundToDouble(PreciseOffset));
}

float UTimeSyncService::CalculateNTPLatency(const FTimeSyncData& SyncData) const
{
    // NTP delay calculation with higher precision:
    // delay = ((t3 - t0) - (t2 - t1)) / 2
    double RoundTrip = static_cast<double>(SyncData.ClientRecvMs - SyncData.ClientSendMs);
    double ServerProcessing = static_cast<double>(SyncData.ServerSendMs - SyncData.ServerRecvMs);
    double PreciseLatency = (RoundTrip - ServerProcessing) / 2.0;
    
    return static_cast<float>(PreciseLatency);
}

float UTimeSyncService::CalculateSampleQuality(const FTimeSyncData& SyncData) const
{
    // Sample quality is based on RTT and server processing time
    // Lower values indicate better quality
    float RTT = static_cast<float>(SyncData.ClientRecvMs - SyncData.ClientSendMs);
    float ServerProcessing = static_cast<float>(SyncData.ServerSendMs - SyncData.ServerRecvMs);
    
    // Quality = RTT + weight * ServerProcessing
    // Higher server processing time is more suspicious
    return RTT + (ServerProcessing * 2.0f);
}

bool UTimeSyncService::IsSampleValid(const FTimeSyncData& SyncData) const
{
    float RTT = static_cast<float>(SyncData.ClientRecvMs - SyncData.ClientSendMs);
    float ServerProcessing = static_cast<float>(SyncData.ServerSendMs - SyncData.ServerRecvMs);
    
    // Check RTT bounds
    if (RTT <= 0 || RTT > MaxValidRTTMs)
    {
        return false;
    }
    
    // Check server processing bounds
    if (ServerProcessing < 0 || ServerProcessing > MaxValidServerProcessingMs)
    {
        return false;
    }
    
    // Check for negative calculated latency (indicates clock issues)
    float CalculatedLatency = CalculateNTPLatency(SyncData);
    if (CalculatedLatency < 0)
    {
        return false;
    }
    
    // Check server processing ratio - if server processing is too large compared to RTT,
    // it indicates the server was overloaded and this sample should be discarded
    float ProcessingRatio = ServerProcessing / RTT;
    if (ProcessingRatio > 0.8f) // Server processing shouldn't be more than 80% of RTT
    {
        return false;
    }

    // NEW: �������� ���������� ������������������ �����������
    //if (SyncData.ServerRecvMs < SyncData.ClientSendMs)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Invalid sample - ServerRecvMs < ClientSendMs"));
    //    return false;
    //}

    //if (SyncData.ServerSendMs < SyncData.ServerRecvMs)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Invalid sample - ServerSendMs < ServerRecvMs"));
    //    return false;
    //}

    //if (SyncData.ClientRecvMs < SyncData.ServerSendMs)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("TimeSyncService: Invalid sample - ClientRecvMs < ServerSendMs (possible clock skew)"));
    //    return false;
    //}
    
    return true;
}

bool UTimeSyncService::IsNearBestSample(EServerType ServerType, const FTimeSyncData& NewSample) const
{
    const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData || ServerData->SyncHistory.Num() < 3)
    {
        return true; // Accept samples when we don't have enough history
    }
    
    // Find the best (lowest) RTT in recent history
    float BestRTT = NewSample.RoundTripTimeMs;
    int32 SamplesToCheck = FMath::Min(ServerData->SyncHistory.Num(), 10); // Check last 10 samples
    
    for (int32 i = ServerData->SyncHistory.Num() - SamplesToCheck; i < ServerData->SyncHistory.Num(); ++i)
    {
        if (i >= 0)
        {
            BestRTT = FMath::Min(BestRTT, ServerData->SyncHistory[i].RoundTripTimeMs);
        }
    }
    
    // Accept sample if it's within 20% of the best RTT
    float RTTThreshold = BestRTT * 1.2f;
    return NewSample.RoundTripTimeMs <= RTTThreshold;
}

void UTimeSyncService::ApplyEWMAFiltering(EServerType ServerType, const FTimeSyncData& NewSample)
{
    FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData)
    {
        return;
    }
    
    float NewOffset = static_cast<float>(NewSample.TimeOffsetMs);
    float NewLatency = NewSample.NetworkLatencyMs;
    
    // Clamp EWMA smoothing factor to valid range
    float Alpha = FMath::Clamp(EWMASmoothingFactor, 0.01f, 0.5f);
    
    if (ServerData->SampleCount == 0)
    {
        // First sample - initialize
        ServerData->FilteredOffsetMs = NewOffset;
        ServerData->FilteredLatencyMs = NewLatency;
    }
    else
    {
        // Calculate the step size and clamp it to prevent large jumps
        float OffsetDelta = NewOffset - ServerData->FilteredOffsetMs;
        float LatencyDelta = NewLatency - ServerData->FilteredLatencyMs;
        
        // Clamp offset/latency change per sample to prevent sudden jumps
        OffsetDelta = FMath::Clamp(OffsetDelta, -MaxOffsetStepMs, MaxOffsetStepMs);
        LatencyDelta = FMath::Clamp(LatencyDelta, -MaxLatencyStepMs, MaxLatencyStepMs);
        
        // Apply clamped EWMA
        ServerData->FilteredOffsetMs += Alpha * OffsetDelta;
        ServerData->FilteredLatencyMs += Alpha * LatencyDelta;
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("TimeSyncService: EWMA %s - NewOffset: %.1f, Delta: %.1f, Filtered: %.1f"), 
            *GetServerTypeName(ServerType), NewOffset, OffsetDelta, ServerData->FilteredOffsetMs);
    }
    
    ServerData->SampleCount++;
}

void UTimeSyncService::UpdateAveragedSyncData(EServerType ServerType)
{
    FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData || ServerData->SyncHistory.Num() == 0)
    {
        return;
    }

    // Update current sync data with EWMA filtered values
    ServerData->CurrentSyncData.NetworkLatencyMs = ServerData->FilteredLatencyMs;
    ServerData->CurrentSyncData.TimeOffsetMs = static_cast<int64>(FMath::RoundToInt(ServerData->FilteredOffsetMs));
    ServerData->CurrentSyncData.ServerType = ServerType;
    
    // Keep the most recent timestamps
    const FTimeSyncData& Latest = ServerData->SyncHistory.Last();
    ServerData->CurrentSyncData.ClientSendMs = Latest.ClientSendMs;
    ServerData->CurrentSyncData.ServerRecvMs = Latest.ServerRecvMs;
    ServerData->CurrentSyncData.ServerSendMs = Latest.ServerSendMs;
    ServerData->CurrentSyncData.ClientRecvMs = Latest.ClientRecvMs;
    ServerData->CurrentSyncData.RoundTripTimeMs = Latest.RoundTripTimeMs;
    ServerData->CurrentSyncData.SampleQuality = Latest.SampleQuality;
}

void UTimeSyncService::EnsureServerSyncDataExists(EServerType ServerType)
{
    if (!ServerSyncData.Contains(ServerType))
    {
        FServerTimeSyncData NewServerData;
        NewServerData.CurrentSyncData.ServerType = ServerType;
        ServerSyncData.Add(ServerType, NewServerData);
    }
}

FString UTimeSyncService::GenerateUniqueRequestId()
{
    // Use combination of current time, counter, and random GUID for uniqueness across all clients
    RequestIdCounter++;
    
    // Add more entropy for better uniqueness across clients
    int64 CurrentTime = GetCurrentClientTimeMs();
    int32 RandomValue = FMath::RandRange(1000, 9999);
    
    // Create a unique ID that includes timestamp, counter, and random component
    FString UniqueId = FString::Printf(TEXT("sync_%lld_%lld_%d_%s"), 
        CurrentTime, 
        RequestIdCounter, 
        RandomValue,
        *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens).Right(8));
    
    return UniqueId;
}

FPendingSyncRequest* UTimeSyncService::FindPendingRequest(const FString& RequestId)
{
    for (FPendingSyncRequest& Request : PendingRequests)
    {
        if (Request.RequestId == RequestId)
        {
            return &Request;
        }
    }
    return nullptr;
}

bool UTimeSyncService::RemovePendingRequest(const FString& RequestId)
{
    for (int32 i = 0; i < PendingRequests.Num(); ++i)
    {
        if (PendingRequests[i].RequestId == RequestId)
        {
            PendingRequests.RemoveAt(i);
            return true;
        }
    }
    return false;
}

//int64 UTimeSyncService::GetSystemTimeMs() const
//{
//    // Get current time in milliseconds since Unix epoch with single call
//    const FDateTime Now = FDateTime::UtcNow();
//    return Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();
//}

//int64 UTimeSyncService::GetSystemTimeMs() const
//{
//    const FDateTime Now = FDateTime::UtcNow();
//    const int64 Result = Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();
//
//    //  ����������� ������������ ���������� �������
//    static int64 LastTime = 0;
//    static int32 BackwardJumps = 0;
//    static int32 LargeJumps = 0;
//    static int32 CallCount = 0;
//
//    CallCount++;
//
//    if (LastTime > 0)
//    {
//        const int64 TimeDiff = Result - LastTime;
//
//        if (TimeDiff < 0)
//        {
//            BackwardJumps++;
//            UE_LOG(LogTemp, Error, TEXT(" SYSTEM TIME BACKWARDS: %lld ms (jumps: %d/%d calls)"),
//                TimeDiff, BackwardJumps, CallCount);
//        }
//        else if (TimeDiff > 1000) // ������ 1 ������� ����� ��������
//        {
//            LargeJumps++;
//            UE_LOG(LogTemp, Warning, TEXT(" SYSTEM TIME LARGE JUMP: %lld ms (jumps: %d/%d calls)"),
//                TimeDiff, LargeJumps, CallCount);
//        }
//        else if (TimeDiff == 0 && CallCount % 100 == 0)
//        {
//            UE_LOG(LogTemp, VeryVerbose, TEXT(" System time resolution: multiple calls return same timestamp"));
//        }
//    }
//
//    // �������� ������ 1000 ������� ��� �������� ��������
//    if (CallCount % 1000 == 0)
//    {
//        UE_LOG(LogTemp, Log, TEXT(" Time quality stats: %d calls, %d backward jumps, %d large jumps"),
//            CallCount, BackwardJumps, LargeJumps);
//    }
//
//    LastTime = Result;
//    return Result;
//}

int64 UTimeSyncService::GetSystemTimeMs() const
{
    // Hybrid approach: periodic wall-clock calibration + high-precision counter
    // between calibrations to avoid FDateTime resolution jitter.
    //
    // Called from both game thread and up to 6 network worker threads
    // concurrently. Calibration is protected by FCriticalSection; the read-only
    // path uses FPlatformAtomics for the int64 anchor and accepts minor skew on
    // the double — a few nanoseconds of jitter in a timestamp is harmless.

    const double CurrentPerfSeconds = FPlatformTime::Seconds();

    // Fast path: read cached anchor without locking.
    const int64  CachedAnchorMs  = FPlatformAtomics::AtomicRead(&AnchorSystemMs);
    const double CachedAnchorSecs = AnchorPerfSeconds;

    const double CalibrationIntervalSec = 5.0;
    const bool bNeedsCalibration = (CachedAnchorMs == 0)
        || ((CurrentPerfSeconds - CachedAnchorSecs) >= CalibrationIntervalSec);

    if (bNeedsCalibration)
    {
        FScopeLock Lock(&CalibrationCs);
        // Re-check under lock to avoid duplicate concurrent calibrations.
        if (AnchorSystemMs == 0 || (CurrentPerfSeconds - AnchorPerfSeconds) >= CalibrationIntervalSec)
        {
            const FDateTime Now = FDateTime::UtcNow();
            FPlatformAtomics::InterlockedExchange(&AnchorSystemMs,
                Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond());
            AnchorPerfSeconds = CurrentPerfSeconds;
        }
    }

    // Between calibrations, use the high-resolution counter for sub-ms stability.
    // Re-read anchor under atomics to see the freshest value from the locked path.
    const int64  FinalAnchorMs  = FPlatformAtomics::AtomicRead(&AnchorSystemMs);
    const double FinalAnchorSecs = AnchorPerfSeconds;
    const double ElapsedSec = CurrentPerfSeconds - FinalAnchorSecs;
    return FinalAnchorMs + static_cast<int64>(ElapsedSec * 1000.0);
}