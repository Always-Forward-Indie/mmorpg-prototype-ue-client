#include "Services/TimeSyncService.h"
#include "Prototyping.h"
#include "Containers/Ticker.h"
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

    RegisterFpsSampler();
}

void UTimeSyncService::RegisterFpsSampler()
{
    // S16b client fps sampler (M4): core ticker, game thread, weak capture.
    // Runs once for the app lifetime (world-independent); re-entry safe.
    if (FpsSamplerHandle.IsValid())
        return;
    TWeakObjectPtr<UTimeSyncService> WeakThis(this);
    FpsSamplerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
        {
            UTimeSyncService* Self = WeakThis.Get();
            if (!Self)
                return false; // service gone: unregister
            Self->FpsWindow.Add(FMath::Max(DeltaTime, 0.00001f));
            Self->FpsWindowSec += DeltaTime;
            if (Self->FpsWindowSec >= 60.0f && Self->FpsWindow.Num() > 0)
            {
                float Sum = 0.0f;
                for (float Dt : Self->FpsWindow)
                    Sum += Dt;
                const float AvgFps = Self->FpsWindow.Num() / FMath::Max(Sum, 0.0001f);
                TArray<float> Sorted = Self->FpsWindow;
                Sorted.Sort();
                const float P95FrameMs = Sorted[FMath::Clamp(
                    FMath::RoundToInt(Sorted.Num() * 0.95f) - 1, 0, Sorted.Num() - 1)] * 1000.0f;
                UE_LOG(LogGameplay, Display, TEXT("CLIENT_FPS avg=%.0f p95frame=%.1fms n=%d"),
                    AvgFps, P95FrameMs, Self->FpsWindow.Num());
                Self->FpsWindow.Reset();
                Self->FpsWindowSec = 0.0f;
            }
            return true;
        }));
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

