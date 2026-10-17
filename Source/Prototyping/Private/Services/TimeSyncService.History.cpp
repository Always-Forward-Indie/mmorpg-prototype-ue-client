#include "Services/TimeSyncService.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Utils/NetCore/TimeSyncMath.h"
#include "Utils/NetCore/SampleRule.h"

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
    // Delegates to the canonical FTimeSyncMath (single source of truth,
    // covered by MMO.TimeSync.NtpMath). Formula unchanged.
    return FTimeSyncMath::OffsetMs(
        SyncData.ClientSendMs, SyncData.ServerRecvMs, SyncData.ServerSendMs, SyncData.ClientRecvMs);
}

float UTimeSyncService::CalculateNTPLatency(const FTimeSyncData& SyncData) const
{
    // Delegates to the canonical FTimeSyncMath (single source of truth,
    // covered by MMO.TimeSync.NtpMath). Formula unchanged.
    return FTimeSyncMath::LatencyMs(
        SyncData.ClientSendMs, SyncData.ServerRecvMs, SyncData.ServerSendMs, SyncData.ClientRecvMs);
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
    // Predicate lives in FSampleRule (pinned by MMO.TimeSync.SampleRule);
    // bounds come from the service tunables. Formula unchanged.
    return FSampleRule::IsValidSample(SyncData.ClientSendMs, SyncData.ServerRecvMs,
        SyncData.ServerSendMs, SyncData.ClientRecvMs, MaxValidRTTMs, MaxValidServerProcessingMs);
}

bool UTimeSyncService::IsNearBestSample(EServerType ServerType, const FTimeSyncData& NewSample) const
{
    const FServerTimeSyncData* ServerData = ServerSyncData.Find(ServerType);
    if (!ServerData || ServerData->SyncHistory.Num() < 3)
    {
        return true; // Accept samples when we don't have enough history
    }

    // Predicate lives in FSampleRule (pinned by MMO.TimeSync.NearBest).
    TArray<float> HistoryRTTs;
    HistoryRTTs.Reserve(ServerData->SyncHistory.Num());
    for (const FTimeSyncData& Sample : ServerData->SyncHistory)
    {
        HistoryRTTs.Add(Sample.RoundTripTimeMs);
    }
    return FSampleRule::IsNearBest(HistoryRTTs, NewSample.RoundTripTimeMs);
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

