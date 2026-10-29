#pragma once

#include "CoreMinimal.h"

/**
 * Travel/readiness timeout policy (pure logic, S08).
 *
 * Canonical owner of the wall-clock decisions in the game-world load flow
 * (TransitionToGameWorld → PostLoadMap → CheckGameWorldReady gates →
 * ReadyFlags → frame countdown → RemoveLoadingScreen):
 * - soft safety: force-remove after LoadingScreenSafetyTimeout (15s default);
 * - hard watchdog: force-remove after 8s wall-clock even before that;
 * - frame countdown: give up waiting for render frames after 3s;
 * - travel crawl diagnostic: flag throttled (unfocused-window) travels.
 *
 * Pure and dependency-free so Automation specs can pin the thresholds.
 * Tickers, delegates and widgets stay in UMyGameInstance.
 */
struct PROTOTYPING_API FTravelGate
{
	/** Soft safety: force-remove the loading screen at/after TimeoutSec. */
	static bool ShouldForceHide(double ElapsedSec, double TimeoutSec) { return ElapsedSec >= TimeoutSec; }

	/** Hard watchdog: unconditional force-remove at/after 8s wall-clock. */
	static constexpr double HardWatchdogSec = 8.0;
	static bool IsHardWatchdogExpired(double ElapsedSec) { return ElapsedSec >= HardWatchdogSec; }

	/** Frame countdown give-up: stop waiting for render frames at/after 3s. */
	static constexpr double FrameCountdownTimeoutSec = 3.0;
	static bool ShouldGiveUpFrameCountdown(double ElapsedSec) { return ElapsedSec >= FrameCountdownTimeoutSec; }

	/** Shader gate cap: in-editor shader compiles must not hold the loading
	 *  screen forever (a stuck compiler would trap every PIE load). After
	 *  this many seconds the gate is bypassed with a warning — packaged
	 *  builds keep waiting (PSO warm-up matters there). */
	static constexpr double ShaderGateCapSec = 8.0;
	static bool ShouldBypassShaderGate(double ElapsedSec) { return ElapsedSec >= ShaderGateCapSec; }

	/** Travel crawl diagnostic: slow wall-clock with (near-)zero rendered frames
	 *  means the engine travel itself was throttled (unfocused window / RHI fence
	 *  waits), not that gates/flags stalled. Used to escalate the Travel DONE log. */
	static bool HasTravelStalled(double TravelSec, uint64 TravelFrames)
	{
		if (TravelSec <= 5.0) { return false; }
		const double Fps = TravelSec > 0.0 ? (double)TravelFrames / TravelSec : 0.0;
		return Fps < 5.0;
	}
};
