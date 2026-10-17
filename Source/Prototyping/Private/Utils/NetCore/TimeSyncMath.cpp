#include "Utils/NetCore/TimeSyncMath.h"

int64 FTimeSyncMath::OffsetMs(int64 T0, int64 T1, int64 T2, int64 T3)
{
	// offset = ((t1 - t0) + (t2 - t3)) / 2
	// Double precision + single rounding at the end: integer division would
	// truncate toward zero and bias negative offsets by 1ms.
	const double ClientToServer = static_cast<double>(T1 - T0);
	const double ServerToClient = static_cast<double>(T2 - T3);
	const double PreciseOffset = (ClientToServer + ServerToClient) / 2.0;

	return static_cast<int64>(FMath::RoundToDouble(PreciseOffset));
}

float FTimeSyncMath::LatencyMs(int64 T0, int64 T1, int64 T2, int64 T3)
{
	// delay = ((t3 - t0) - (t2 - t1)) / 2
	const double RoundTrip = static_cast<double>(T3 - T0);
	const double ServerProcessing = static_cast<double>(T2 - T1);
	const double PreciseLatency = (RoundTrip - ServerProcessing) / 2.0;

	return static_cast<float>(PreciseLatency);
}
