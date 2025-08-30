# Time Synchronization Service Documentation

## Overview

The `TimeSyncService` provides client-server time synchronization for lag compensation in network gameplay. It automatically handles time differences between client and server, enabling accurate prediction and compensation for network latency.

## Architecture

### Components

1. **TimeSyncService** - Core service managing time synchronization
2. **NetworkManager** - Automatically processes time sync data from server responses
3. **JSONParser** - Enhanced to automatically add `clientSendMs` to all requests
4. **SkillSystemManager** - Uses synchronized time for accurate cooldown calculations

### Data Structures

```cpp
struct FTimeSyncData
{
    int64 ClientSendMs;      // Client timestamp when request was sent
    int64 ServerRecvMs;      // Server timestamp when request was received  
    int64 ServerSendMs;      // Server timestamp when response was sent
    int64 ClientRecvMs;      // Client timestamp when response was received
    float NetworkLatencyMs; // Calculated network latency (RTT/2)
    int64 TimeOffsetMs;     // Time offset between client and server
};
```

### Message Format

All client-server messages now include time synchronization data:

**Client Request:**
```json
{
  "header": {
    "clientSendMs": 1756409198901,
    "eventType": "moveCharacter",
    "clientId": 123,
    "hash": "abc123"
  },
  "body": {
    // ... request data
  }
}
```

**Server Response:**
```json
{
  "header": {
    "serverRecvMs": 1756409198952,
    "serverSendMs": 1756409199000,
    "clientSendMsEcho": 1756409198901,
    "eventType": "moveCharacter",
    "status": "success"
  },
  "body": {
    // ... response data
  }
}
```

## Integration Guide

### 1. Automatic Integration

Time synchronization is automatically integrated into the following systems:

- **NetworkManager**: Processes time sync data from all server responses
- **JSONParser**: Automatically adds `clientSendMs` to all outgoing requests
- **HarvestManager**: Uses `SerializeJsonWithTimeSync` for harvest requests
- **CombatSystemManager**: Uses `SerializeJsonWithTimeSync` for attack requests
- **SkillSystemManager**: Uses synchronized time for cooldown calculations

### 2. Using TimeSyncService in Your Code

#### Basic Usage

```cpp
// Get service instance
UTimeSyncService* TimeSyncService = GameInstance->GetTimeSyncService();

// Get current synchronized server time
int64 ServerTime = TimeSyncService->GetEstimatedServerTimeMs();

// Calculate lag compensation
float LagCompensation = TimeSyncService->CalculateLagCompensation(ClientActionTimeMs);
```

#### Manual Time Sync Updates

```cpp
// Update time sync data (usually done automatically by NetworkManager)
TimeSyncService->UpdateTimeSyncData(ClientSendMs, ServerRecvMs, ServerSendMs);

// Check if sync data is valid
if (TimeSyncService->IsTimeSyncValid())
{
    // Use synchronized time
    float Latency = TimeSyncService->GetNetworkLatencyMs();
    int64 Offset = TimeSyncService->GetTimeOffsetMs();
}
```

#### Custom JSON Serialization

```cpp
// Automatic (recommended)
FString JsonRequest = JSONParser::SerializeJson(EventType, HeaderData, BodyData);
// clientSendMs is automatically added

// Manual with specific TimeSyncService
FString JsonRequest = JSONParser::SerializeJsonWithTimeSync(EventType, HeaderData, BodyData, TimeSyncService);
```

### 3. Lag Compensation Examples

#### Movement Prediction

```cpp
// Calculate where player should be accounting for lag
int64 PlayerActionTime = GetPlayerLastInputTime();
float LagMs = TimeSyncService->CalculateLagCompensation(PlayerActionTime);
FVector PredictedPosition = InterpolatePosition(LastPosition, CurrentPosition, LagMs);
```

#### Skill Cooldowns

```cpp
// Use synchronized time for accurate cooldowns
float CurrentTime = SkillSystemManager->GetSynchronizedWorldTime();
bool bOnCooldown = (CurrentTime < SkillEndTime);
```

#### Combat Hit Detection

```cpp
// Compensate for network delay in hit detection
int64 ClientHitTime = GetClientHitTimestamp();
int64 ServerHitTime = TimeSyncService->ClientTimeToServerTime(ClientHitTime);
float TimeDifference = (GetCurrentServerTime() - ServerHitTime) / 1000.0f;
```

## Configuration

### Service Settings

```cpp
// Maximum age of sync data before considered stale (default: 30 seconds)
TimeSyncService->MaxSyncAgeMs = 30000;

// Minimum interval between sync requests (default: 5 seconds)  
TimeSyncService->MinSyncIntervalMs = 5000;

// Number of sync samples to average (default: 10)
TimeSyncService->MaxSyncSamples = 10;
```

### Debug Commands

```cpp
// Log current time sync statistics
TimeSyncService->LogTimeSyncStats();

// Force time sync update
TimeSyncService->RequestTimeSync();
```

## Implementation Details

### Time Calculation

1. **Network Latency**: `(ClientRecvMs - ClientSendMs) / 2`
2. **Time Offset**: `ServerRecvMs - NetworkLatency - ClientSendMs`
3. **Server Time Estimation**: `ClientTimeMs + TimeOffset`

### Averaging

The service maintains a rolling average of the last N sync samples to smooth out network jitter and provide more stable time estimates.

### Automatic Cleanup

- Invalid sync data is automatically cleaned up
- Stale sync data is detected and triggers new sync requests
- Network responses automatically update sync data

## Best Practices

### 1. Use Synchronized Time for Game Logic

```cpp
// Good - Use synchronized time
float GameTime = SkillSystemManager->GetSynchronizedWorldTime();

// Avoid - Local time only
float LocalTime = GetWorld()->GetTimeSeconds();
```

### 2. Check Sync Validity

```cpp
if (TimeSyncService->IsTimeSyncValid())
{
    // Safe to use synchronized time
    ProcessWithLagCompensation();
}
else
{
    // Fallback to local time or request sync
    ProcessLocally();
}
```

### 3. Handle Edge Cases

```cpp
// Always check for null TimeSyncService
UTimeSyncService* TimeSyncService = GetTimeSyncService();
if (TimeSyncService && TimeSyncService->IsTimeSyncValid())
{
    // Use synchronized time
}
else
{
    // Fallback behavior
}
```

## System Integration Status

| System | Integration Status | Notes |
|--------|-------------------|-------|
| NetworkManager | ? Complete | Automatic time sync processing |
| JSONParser | ? Complete | Auto-adds clientSendMs |
| HarvestManager | ? Complete | Updated all request methods |
| CombatSystemManager | ? Complete | Attack requests with sync |
| SkillSystemManager | ? Complete | Synchronized cooldowns |
| ItemManager | ?? Pending | Needs SerializeJsonWithTimeSync |
| PlayerManager | ?? Pending | Needs SerializeJsonWithTimeSync |
| AuthenticationManager | ?? Pending | Needs SerializeJsonWithTimeSync |
| InventoryManager | ?? Pending | Needs SerializeJsonWithTimeSync |

## Troubleshooting

### Common Issues

1. **Time Sync Not Working**
   - Check if TimeSyncService is initialized in MyGameInstance
   - Verify server is sending time sync fields in responses
   - Check logs for sync update messages

2. **High Latency Values**
   - Verify network connection quality
   - Check if sync samples are being averaged correctly
   - Consider adjusting MaxSyncSamples

3. **Stale Sync Data**
   - Increase MinSyncIntervalMs if getting too many sync requests
   - Check MaxSyncAgeMs setting
   - Verify network responses are being processed

### Debug Logging

Enable verbose logging to see time sync operations:

```cpp
// In Development builds
UE_LOG(LogTemp, VeryVerbose, TEXT("TimeSyncService debug info"));
```

## Future Enhancements

1. **Adaptive Sync Frequency**: Adjust sync frequency based on network stability
2. **Prediction Rollback**: Implement client-side prediction with server reconciliation
3. **Clock Drift Compensation**: Handle gradual time drift between client and server
4. **Multiple Server Support**: Sync with different game servers independently