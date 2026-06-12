#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TimeSyncService.generated.h"

// Enum for different server types
UENUM(BlueprintType)
enum class EServerType : uint8
{
    LoginServer     UMETA(DisplayName = "Login Server"),
    GameServer      UMETA(DisplayName = "Game Server"),
    ChunkServer     UMETA(DisplayName = "Chunk Server")
};

// Time sync data structure for lag compensation
USTRUCT(BlueprintType)
struct FTimeSyncData
{
    GENERATED_BODY()

    // NTP-style 4 timestamps (t0, t1, t2, t3)
    // t0: Client timestamp when request was sent
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 ClientSendMs = 0;

    // t1: Server timestamp when request was received
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 ServerRecvMs = 0;

    // t2: Server timestamp when response was sent
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 ServerSendMs = 0;

    // t3: Client timestamp when response was received (calculated on client)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 ClientRecvMs = 0;

    // Calculated network latency using NTP formula
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float NetworkLatencyMs = 0.0f;

    // Calculated time offset between client and server using NTP formula
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 TimeOffsetMs = 0;

    // Round trip time for this sample
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float RoundTripTimeMs = 0.0f;

    // Quality metric for this sample (lower is better)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float SampleQuality = 0.0f;

    // Server type this sync data belongs to
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    EServerType ServerType = EServerType::ChunkServer;

    FTimeSyncData()
    {
        ClientSendMs = 0;
        ServerRecvMs = 0;
        ServerSendMs = 0;
        ClientRecvMs = 0;
        NetworkLatencyMs = 0.0f;
        TimeOffsetMs = 0;
        RoundTripTimeMs = 0.0f;
        SampleQuality = 0.0f;
        ServerType = EServerType::ChunkServer;
    }
};

// Structure for tracking pending sync requests
USTRUCT()
struct FPendingSyncRequest
{
    GENERATED_BODY()

    // Unique request ID
    UPROPERTY()
    FString RequestId;

    // Server type
    UPROPERTY()
    EServerType ServerType = EServerType::ChunkServer;

    // Client timestamp when request was sent
    UPROPERTY()
    int64 ClientSendMs = 0;

    // Timeout timestamp
    UPROPERTY()
    int64 TimeoutMs = 0;

    FPendingSyncRequest()
    {
        RequestId = TEXT("");
        ServerType = EServerType::ChunkServer;
        ClientSendMs = 0;
        TimeoutMs = 0;
    }

    FPendingSyncRequest(const FString& InRequestId, EServerType InServerType, int64 InClientSendMs, int64 InTimeoutMs)
        : RequestId(InRequestId)
        , ServerType(InServerType)
        , ClientSendMs(InClientSendMs)
        , TimeoutMs(InTimeoutMs)
    {
    }
};

// Server-specific sync data container
USTRUCT(BlueprintType)
struct FServerTimeSyncData
{
    GENERATED_BODY()

    // Current sync data for this server
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    FTimeSyncData CurrentSyncData;

    // History of sync samples for averaging
    UPROPERTY()
    TArray<FTimeSyncData> SyncHistory;

    // Timestamp of last sync request
    UPROPERTY()
    int64 LastSyncRequestMs = 0;

    // EWMA filtered offset and latency
    UPROPERTY()
    float FilteredOffsetMs = 0.0f;

    UPROPERTY()
    float FilteredLatencyMs = 0.0f;

    // Sample count for initialization
    UPROPERTY()
    int32 SampleCount = 0;

    FServerTimeSyncData()
    {
        CurrentSyncData = FTimeSyncData();
        SyncHistory.Empty();
        LastSyncRequestMs = 0;
        FilteredOffsetMs = 0.0f;
        FilteredLatencyMs = 0.0f;
        SampleCount = 0;
    }
};

/**
 * Service for managing time synchronization between client and multiple servers
 * Used for lag compensation in movement, combat, cooldowns, etc.
 * Supports separate synchronization for Login, Game, and Chunk servers
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UTimeSyncService : public UObject
{
    GENERATED_BODY()

public:
    UTimeSyncService();

    // Initialize the time sync service
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    void Initialize();

    // Set world context for timer management
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    void SetWorldContext(UWorld* World);

    // Get current client timestamp in milliseconds
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    int64 GetCurrentClientTimeMs() const;

    // Get estimated server timestamp based on current client time and sync data
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    int64 GetEstimatedServerTimeMs(EServerType ServerType = EServerType::ChunkServer) const;

    // Get network latency in milliseconds for specific server
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    float GetNetworkLatencyMs(EServerType ServerType = EServerType::ChunkServer) const;

    // Get time offset between client and server for specific server
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    int64 GetTimeOffsetMs(EServerType ServerType = EServerType::ChunkServer) const;

    // Convert client time to estimated server time for specific server
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    int64 ClientTimeToServerTime(int64 ClientTimeMs, EServerType ServerType = EServerType::ChunkServer) const;

    // Convert server time to estimated client time for specific server
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    int64 ServerTimeToClientTime(int64 ServerTimeMs, EServerType ServerType = EServerType::ChunkServer) const;

    // Calculate lag compensation for a given client timestamp for specific server
    // Returns how much time has passed on server since the client action
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    float CalculateLagCompensation(int64 ClientActionTimeMs, EServerType ServerType = EServerType::ChunkServer) const;

    // Generate unique request ID and prepare for time sync.
    // Pass InClientSendMs to pin t0 to the moment the packet is actually sent,
    // so the NTP formula uses the same t0 that was embedded in the JSON header.
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    FString GenerateAndRegisterSyncRequest(EServerType ServerType = EServerType::ChunkServer, int64 InClientSendMs = 0);

    // Update time sync data when receiving server response with specific request ID
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    bool UpdateTimeSyncData(const FString& RequestId, int64 ServerRecvMs, int64 ServerSendMs);

    // Check if time sync data is valid and recent for specific server
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    bool IsTimeSyncValid(EServerType ServerType = EServerType::ChunkServer) const;

    // Force time sync update (request time sync from server)
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    void RequestTimeSync(EServerType ServerType = EServerType::ChunkServer);

    // Get the current time sync data for specific server
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    FTimeSyncData GetCurrentTimeSyncData(EServerType ServerType = EServerType::ChunkServer) const;

    // Debug function to log time sync statistics for specific server
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    void LogTimeSyncStats(EServerType ServerType = EServerType::ChunkServer) const;

    // Debug function to log all servers' time sync statistics
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    void LogAllServersTimeSyncStats() const;

    // Cleanup expired pending requests
    UFUNCTION(BlueprintCallable, Category = "Time Sync")
    void CleanupExpiredRequests();

    // Get server name for logging
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Time Sync")
    FString GetServerTypeName(EServerType ServerType) const;

protected:
    // Time sync data for each server type
    UPROPERTY(BlueprintReadOnly, Category = "Time Sync")
    TMap<EServerType, FServerTimeSyncData> ServerSyncData;

    // Pending sync requests awaiting response
    UPROPERTY()
    TArray<FPendingSyncRequest> PendingRequests;

    // Maximum number of sync samples to keep per server
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int32 MaxSyncSamples = 10;

    // Maximum age of sync data in milliseconds before it's considered stale
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 MaxSyncAgeMs = 30000; // 30 seconds

    // Minimum interval between time sync requests in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 MinSyncIntervalMs = 5000; // 5 seconds

    // Request timeout in milliseconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    int64 RequestTimeoutMs = 10000; // 10 seconds

    // Counter for generating unique request IDs
    UPROPERTY()
    int64 RequestIdCounter = 0;

    // EWMA smoothing factor (0.0 - 1.0, lower = more smoothing)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float EWMASmoothingFactor = 0.125f; // 1/8, similar to NTP

    // Maximum RTT to consider a sample valid (in ms)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float MaxValidRTTMs = 1000.0f; // 1 second

    // Maximum server processing time to consider valid (in ms)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float MaxValidServerProcessingMs = 100.0f; // 100ms

    // Maximum offset step change per sample (in ms)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float MaxOffsetStepMs = 50.0f; // 50ms

    // Maximum latency step change per sample (in ms)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time Sync")
    float MaxLatencyStepMs = 20.0f; // 20ms

    // Calculate average latency from sync history for specific server
    float CalculateAverageLatency(EServerType ServerType) const;

    // Calculate average time offset from sync history for specific server
    int64 CalculateAverageTimeOffset(EServerType ServerType) const;

    // Add sync data to history and maintain max samples for specific server
    void AddSyncDataToHistory(EServerType ServerType, const FTimeSyncData& SyncData);

    // Update averaged sync data from history for specific server
    void UpdateAveragedSyncData(EServerType ServerType);

    // Initialize server sync data if not exists
    void EnsureServerSyncDataExists(EServerType ServerType);

    // Generate unique request ID
    FString GenerateUniqueRequestId();

    // Find pending request by ID
    FPendingSyncRequest* FindPendingRequest(const FString& RequestId);

    // Remove pending request by ID
    bool RemovePendingRequest(const FString& RequestId);

    // NTP-style time offset calculation
    int64 CalculateNTPOffset(const FTimeSyncData& SyncData) const;

    // NTP-style latency calculation  
    float CalculateNTPLatency(const FTimeSyncData& SyncData) const;

    // Calculate sample quality (RTT + server processing time)
    float CalculateSampleQuality(const FTimeSyncData& SyncData) const;

    // Check if sample is valid based on quality metrics
    bool IsSampleValid(const FTimeSyncData& SyncData) const;

    // Check if sample is near the best samples (low latency) in recent history
    bool IsNearBestSample(EServerType ServerType, const FTimeSyncData& NewSample) const;

    // Apply EWMA filtering to offset and latency
    void ApplyEWMAFiltering(EServerType ServerType, const FTimeSyncData& NewSample);

private:
    // Internal helper to get system time in milliseconds
    int64 GetSystemTimeMs() const;

    // Timer handle for automatic cleanup
    FTimerHandle CleanupTimerHandle;

    // World context for timer management
    UPROPERTY()
    TObjectPtr<UWorld> WorldContext;
};