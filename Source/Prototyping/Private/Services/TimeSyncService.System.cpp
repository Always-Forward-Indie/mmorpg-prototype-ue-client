#include "Services/TimeSyncService.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Utils/NetCore/TimeSyncMath.h"

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
