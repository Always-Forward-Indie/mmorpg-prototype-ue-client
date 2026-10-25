#include "DevMode/DevModeConsoleCommands.h"
#include "Prototyping.h"
#include "DevMode/DevModeBotBrain.h"
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "DevMode/DevModeDataProvider.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "HighResScreenshot.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

void UDevModeConsoleCommands::PerfReset() const
{
    // (Re)start the sampling window: core-ticker deltas (cap keeps a long
    // idle session from growing the buffer) + frame/time anchors.
    PerfSamples.Reset();
    PerfFramesStart = GFrameCounter;
    PerfTimeStart = FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().RemoveTicker(PerfSamplerHandle);
    TWeakObjectPtr<const UDevModeConsoleCommands> WeakThis(this);
    PerfSamplerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([WeakThis](float Dt)
        {
            const UDevModeConsoleCommands* Self = WeakThis.Get();
            if (!Self) { return false; }
            if (Self->PerfSamples.Num() < 7200)
            {
                Self->PerfSamples.Add(Dt);
            }
            return true;
        }), 0.0f);
}

void UDevModeConsoleCommands::PerfMark(const FString& Phase) const
{
    const uint64 Frames = (GFrameCounter >= PerfFramesStart) ? (GFrameCounter - PerfFramesStart) : 0;
    const double Sec = FPlatformTime::Seconds() - PerfTimeStart;
    const double Fps = (Sec > 0.001 && Frames > 0) ? (double)Frames / Sec : 0.0;
    // p95 frame time over the window (ms). Sorted copy — windows are small.
    float P95Ms = 0.0f;
    if (PerfSamples.Num() > 0)
    {
        TArray<float> Sorted = PerfSamples;
        Sorted.Sort();
        P95Ms = Sorted[FMath::Clamp(FMath::RoundToInt(Sorted.Num() * 0.95f), 0, Sorted.Num() - 1)] * 1000.0f;
    }
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_PERF phase=%s frames=%llu seconds=%.2f fps=%.1f p95ms=%.1f"),
        *Phase, Frames, Sec, Fps, P95Ms);
    // Restart the window for the next phase; the sampler keeps ticking.
    PerfSamples.Reset();
    PerfFramesStart = GFrameCounter;
    PerfTimeStart = FPlatformTime::Seconds();
}

void UDevModeConsoleCommands::PerfStop() const
{
    FTSTicker::GetCoreTicker().RemoveTicker(PerfSamplerHandle);
    PerfSamplerHandle.Reset();
}

void UDevModeConsoleCommands::ExpectGrounded() const
{
    // Void falls must fail the sweep LOUD (2D asserts would pass mid-air).
    // Called one phase after each teleport so the drop has time to settle.
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    const UCharacterMovementComponent* MC = Player ? Player->GetCharacterMovement() : nullptr;
    const bool bGrounded = MC && MC->IsMovingOnGround();
    if (bGrounded)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_ASSERT PASS grounded eq 1 actual=1"));
    }
    else
    {
        const FVector At = Player ? Player->GetActorLocation() : FVector::ZeroVector;
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_ASSERT FAIL grounded eq 1 actual=0 at (%.0f,%.0f,%.0f)"), At.X, At.Y, At.Z);
        ++SweepFailures;
    }
}

void UDevModeConsoleCommands::SweepShot(const FString& Tag) const
{
    // Screenshots only make sense with a renderer; nightly nullrhi runs skip them.
    if (!FParse::Param(FCommandLine::Get(), TEXT("devmode-shots")))
    {
        return;
    }
    FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("DevSweep_%s"), *Tag), true, true);
    UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_SWEEP_SHOT %s"), *Tag);
}
