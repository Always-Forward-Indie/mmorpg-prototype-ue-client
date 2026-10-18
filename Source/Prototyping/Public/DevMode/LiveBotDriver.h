#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Authentication/LoginFlowTypes.h"
#include "LiveBotDriver.generated.h"

class UMyGameInstance;
class UDevModeBotBrain;

// ============================================================================
// ULiveBotDriver — L1-live auto-login + join driver. DEV-ONLY test path.
//
// Activated ONLY by the -bot-live=<scenario> command line (plus
// -botlogin=<user> -botpass=<pass>). Without those flags this object is never
// created and online flow is byte-for-byte unchanged.
//
// Drives the REAL online flow without typing into widgets (typing is Slate,
// already covered manually): waits for the login-server socket, sends
// authentificationClient, requests getCharactersList, picks the FIRST
// character, calls JoinSelectedCharacterToGame, waits for the game world +
// local player spawn, then hands control to UDevModeBotBrain in LIVE mode
// (real server damage — no BOT_HIT_STUB; no spawn teleport).
//
// Credentials come from the command line and are NEVER logged (username only).
// Every state has a timeout plus a global watchdog — always ends with a
// LIVEBOT_RESULT tombstone and client quit, never hangs CI.
// Tombstones for log scan: "LIVEBOT_BEGIN <scenario> <user>",
// "LIVEBOT_RESULT <scenario> <PASS|FAIL> ...".
// ============================================================================
UCLASS()
class PROTOTYPING_API ULiveBotDriver : public UObject
{
    GENERATED_BODY()

public:
    // Begins the live run. Reads -botlogin=/-botpass= itself.
    void Start(UMyGameInstance* InGameInstance, const FString& InScenario);
    void Stop();
    bool IsRunning() const { return bRunning; }

private:
    enum class ELiveStep : uint8
    {
        WaitConnect,
        WaitAuth,
        WaitChars,
        WaitWorld,
        // Roundtrip supervisor: brain leg 1 finished (no quit); re-run the
        // connect/auth/chars/join flow for leg 2 with the same creds.
        WaitBrainLeg1,
    };

    UFUNCTION()
    void PollTick();

    UFUNCTION()
    void HandleLoginResponse(bool bSuccess, const FString& Message);

    UFUNCTION()
    void HandleCharacterList(const TArray<FLoginCharacterEntry>& Characters);

    void HandlePostLoadMap(UWorld* LoadedWorld);

    void Finish(bool bPass, const FString& Reason);
    void QuitClient() const;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    // GC root for the live brain. A bare NewObject local is UNREACHABLE to
    // the collector (timers hold only weak refs) — LoadMap GC passes then
    // kill the timers silently: no ticks, no timeouts, no tombstone, client
    // idles until CI kills it. The offline brain is rooted the same way via
    // GameInstance->DevModeBotBrain. Found live 2026-10-15: live_loot_pickup
    // ticked 8s (kill confirmed) then went silent with zero log output.
    UPROPERTY()
    UDevModeBotBrain* LiveBrain = nullptr;

    // Current world tracked via PostLoadMapWithWorld — NEVER GameInstance->
    // GetWorld() from this ticker: during travel the UGameInstance world
    // context dangles (use-after-free AV reading 0xFF..). Weak ptr goes null
    // across travel instead of crashing; the poll simply waits it out.
    TWeakObjectPtr<UWorld> CurrentWorld;
    FDelegateHandle PostLoadMapHandle;
    // When CurrentWorld was last seen null (transit tolerance).
    double NullWorldSince = 0.0;

    // Core ticker (NOT a world timer): the driver spans the login→game travel
    // and world TimerManagers die with their worlds. Wall-clock timeouts.
    FTSTicker::FDelegateHandle PollTickerHandle;

    FString Scenario;
    FString Login;
    FString Password;
    ELiveStep Step = ELiveStep::WaitConnect;
    double StepStartTime = 0.0;
    double RunStartTime = 0.0;
    double LastConnectAttemptTime = 0.0;
    bool bLoginSent = false;
    bool bCharsRequested = false;
    bool bRunning = false;

    // Roundtrip state (live_travel_roundtrip only): supervisor reuses the
    // connect/auth/chars/join flow for leg 2 instead of quitting at login.
    bool bTravelRoundtrip = false;
    int32 RoundtripLeg = 1;
    int32 Leg1CharId = 0;
    // Once leg 2 starts, the leg-1 watchdog below must stand down: the leg-2
    // verify brain owns its own timeouts + tombstone, and its normal Finish
    // (followed by auto-quit) must not be misread as a leg-1 death.
    bool bRoundtripLeg2Active = false;
    // Whole-run budget, set in Start() (default GlobalTimeoutSec;
    // roundtrip needs ~2 legs so it gets a bigger budget there).
    float RunTimeoutSec;

    // State budgets (seconds). World wait is generous: travel crawls on a
    // throttled window (measured ~3-10 fps unfocused).
    static constexpr float ConnectTimeoutSec = 60.0f;
    static constexpr float AuthTimeoutSec = 30.0f;
    static constexpr float CharsTimeoutSec = 30.0f;
    static constexpr float WorldTimeoutSec = 240.0f;
    static constexpr float GlobalTimeoutSec = 360.0f;
};
