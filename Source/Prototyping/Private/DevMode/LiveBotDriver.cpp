#include "DevMode/LiveBotDriver.h"
#include "MyGameInstance.h"
#include "Authentication/AuthenticationManager.h"
#include "Networking/NetworkManager.h"
#include "DevMode/DevModeBotBrain.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Misc/Parse.h"

void ULiveBotDriver::Start(UMyGameInstance* InGameInstance, const FString& InScenario)
{
    if (bRunning) { return; }
    GameInstance = InGameInstance;
    UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
    if (!GameInstance || !World)
    {
        UE_LOG(LogTemp, Warning, TEXT("LIVEBOT_RESULT %s FAIL no-world"), *InScenario);
        return;
    }

    // Credentials: command line only, dev-only path. Password never logged.
    FParse::Value(FCommandLine::Get(), TEXT("botlogin="), Login);
    FParse::Value(FCommandLine::Get(), TEXT("botpass="), Password);
    if (Login.IsEmpty() || Password.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("LIVEBOT_RESULT %s FAIL missing-creds (need -botlogin= -botpass=)"), *InScenario);
        return;
    }

    Scenario = InScenario;
    Step = ELiveStep::WaitConnect;
    StepStartTime = FPlatformTime::Seconds();
    RunStartTime = StepStartTime;
    bLoginSent = false;
    bCharsRequested = false;
    bRunning = true;
    // Roundtrip needs ~2 full login→game legs: generous whole-run budget.
    bTravelRoundtrip = Scenario.Equals(TEXT("live_travel_roundtrip"), ESearchCase::IgnoreCase);
    RoundtripLeg = 1;
    Leg1CharId = 0;
    RunTimeoutSec = bTravelRoundtrip ? 600.0f : GlobalTimeoutSec;

    if (UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager())
    {
        Auth->OnLoginResponse.AddDynamic(this, &ULiveBotDriver::HandleLoginResponse);
        Auth->OnCharacterListReceived.AddDynamic(this, &ULiveBotDriver::HandleCharacterList);
    }

    // Track the world via PostLoadMap, never via GameInstance->GetWorld()
    // (dangles across travel — use-after-free). Starts at the login world.
    CurrentWorld = World;
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &ULiveBotDriver::HandlePostLoadMap);

    // Core ticker: survives the login→game level travel (world TimerManagers
    // are destroyed with their worlds). Self-unregisters when Stop() runs.
    FTSTicker::GetCoreTicker().RemoveTicker(PollTickerHandle);
    PollTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float)
        {
            PollTick();
            return bRunning;
        }),
        0.5f);

    UE_LOG(LogTemp, Warning, TEXT("LIVEBOT_BEGIN %s user='%s'"), *Scenario, *Login);
}

void ULiveBotDriver::Stop()
{
    if (GameInstance)
    {
        if (UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager())
        {
            Auth->OnLoginResponse.RemoveDynamic(this, &ULiveBotDriver::HandleLoginResponse);
            Auth->OnCharacterListReceived.RemoveDynamic(this, &ULiveBotDriver::HandleCharacterList);
        }
    }
    FTSTicker::GetCoreTicker().RemoveTicker(PollTickerHandle);
    PollTickerHandle.Reset();
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }
    bRunning = false;
}

void ULiveBotDriver::HandlePostLoadMap(UWorld* LoadedWorld)
{
    // Fires for EVERY travel (including the boot map and failures with
    // nullptr) — just track, never assume.
    CurrentWorld = LoadedWorld;
    NullWorldSince = 0.0;
}

void ULiveBotDriver::PollTick()
{
    if (!bRunning) { return; }

    // Never GameInstance->GetWorld() here: its world context dangles across
    // travel (use-after-free AV). The weak ptr simply goes null in transit.
    UWorld* World = CurrentWorld.Get();
    if (!World || World->bIsTearingDown)
    {
        if (NullWorldSince <= 0.0) { NullWorldSince = FPlatformTime::Seconds(); }
        if (FPlatformTime::Seconds() - NullWorldSince > 60.0)
        {
            Finish(false, TEXT("lost-world"));
        }
        return;
    }
    NullWorldSince = 0.0;

    const double Now = FPlatformTime::Seconds();

    if (Now - RunStartTime > RunTimeoutSec)
    {
        Finish(false, TEXT("global-timeout"));
        return;
    }

    if (Step == ELiveStep::WaitConnect)
    {
        UNetworkManager* Net = GameInstance->GetNetworkManager();
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (Net && Auth && Net->IsLoginServerConnected())
        {
            Auth->SendLoginRequest(Login, Password);
            bLoginSent = true;
            Step = ELiveStep::WaitAuth;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("LIVEBOT login sent user='%s'"), *Login);
            return;
        }
        // Re-arm the connection every 10s: NetworkManager gives up after a few
        // fast retries (human-oriented popup flow); CI must survive hiccups.
        if (Net && Now - LastConnectAttemptTime > 10.0)
        {
            LastConnectAttemptTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("LIVEBOT (re)connecting to login server..."));
            Net->ConnectLoginServer();
        }
        if (Now - StepStartTime > ConnectTimeoutSec)
        {
            Finish(false, TEXT("connect-timeout"));
        }
        return;
    }

    if (Step == ELiveStep::WaitAuth)
    {
        if (Now - StepStartTime > AuthTimeoutSec)
        {
            Finish(false, TEXT("auth-timeout"));
        }
        return;
    }

    if (Step == ELiveStep::WaitChars)
    {
        if (Now - StepStartTime > CharsTimeoutSec)
        {
            Finish(false, TEXT("chars-timeout"));
        }
        return;
    }

    if (Step == ELiveStep::WaitBrainLeg1)
    {
        // Roundtrip supervisor: leg 1 finished at login (no quit). Tear the
        // leg-1 brain down and re-run the proven connect/auth/chars/join
        // flow for leg 2 with the same creds.
        if (LiveBrain && LiveBrain->bRoundtripLeg1Done)
        {
            UE_LOG(LogTemp, Warning, TEXT("LIVEBOT roundtrip leg 1 done, starting leg 2"));
            LiveBrain->Stop();
            Leg1CharId = GameInstance ? GameInstance->GetCurrentCharacterID() : 0;
            RoundtripLeg = 2;
            bLoginSent = false;
            bCharsRequested = false;
            Step = ELiveStep::WaitConnect;
            StepStartTime = Now;
            LastConnectAttemptTime = 0.0;
            return;
        }
        if (LiveBrain && !LiveBrain->IsRunning())
        {
            // Brain died without reporting leg-1 completion — no tombstone
            // to trust; fail loudly instead of hanging to global timeout.
            Finish(false, TEXT("roundtrip-leg1-died"));
            return;
        }
        return;
    }

    if (Step == ELiveStep::WaitWorld)
    {
        ABasicPlayer* Player = GameInstance->Player;
        UWorld* TrackedWorld = CurrentWorld.Get();
        if (GameInstance->IsGameWorldReady() && Player && IsValid(Player) && !Player->IsActorBeingDestroyed() && TrackedWorld)
        {
            // Diagnostic: the run uses TrackedWorld (PostLoadMap-tracked), never
            // GameInstance->GetWorld() (stale across travel -> dead timers).
            UE_LOG(LogTemp, Warning, TEXT("LIVEBOT world ready, starting brain (live mode: real damage, no teleport), world='%s'"),
                *TrackedWorld->GetName());
            // Rooted on the driver (UPROPERTY): a bare local would be GC'd
            // mid-run, killing the brain timers silently (no tombstone).
            LiveBrain = NewObject<UDevModeBotBrain>(GameInstance);
            UDevModeBotBrain* Brain = LiveBrain;
            Brain->ScenarioName = Scenario;
            Brain->TombstonePrefix = TEXT("LIVEBOT");
            Brain->bTeleportToSpawn = false;
            Brain->bStubHits = false;
            Brain->bAutoQuitAtFinish = true;
            Brain->bDirectMove = true;
            Brain->GlobalTimeoutSec = 300.0f;
            Brain->bRendezvousMode = Scenario.Equals(TEXT("live_cross_visibility"), ESearchCase::IgnoreCase);
            // Phase A: one brain, different flags. Loot kills one mob for
            // real then picks up the real server drop; death stands down
            // and waits for real server damage + real respawn round-trip.
            Brain->bLootPhase = false;
            Brain->bStubLoot = false;
            Brain->bDeathPhase = false;
            Brain->bStubDeath = false;
            Brain->bPassiveCombat = false;
            Brain->bHarvestPhase = false;
            Brain->bStubHarvest = false;
            Brain->bVendorPhase = false;
            Brain->bChatPhase = false;
            Brain->bQuestPhase = false;
            Brain->bSkillsPhase = false;
            Brain->bEquipPhase = false;
            Brain->bLevelPhase = false;
            Brain->bTitleEmotePhase = false;
            Brain->bPeripheryPhase = false;
            Brain->bCharFlowPhase = false;
            Brain->bBackToLoginPhase = false;
            // Cross runs talk after meeting (plan Phase C): both sides send
            // one local message and assert the peer's arrival + bubble.
            // (Set after the resets above, which clear it.)
            Brain->bChatPhase = Brain->bRendezvousMode;
            if (Scenario.Equals(TEXT("live_loot_pickup"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 1;
                Brain->bLootPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_death_respawn"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bDeathPhase = true;
                Brain->bPassiveCombat = true;
            }
            else if (Scenario.Equals(TEXT("live_harvest_run"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 1;
                Brain->bLootPhase = true;
                Brain->bHarvestPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_vendor_repair"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bVendorPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_quest_turnin"), ESearchCase::IgnoreCase))
            {
                // Full chain (dialogue + 2 real kills + harvest + loot +
                // turnin) is slow: generous global budget on top of the
                // per-leg timeouts.
                Brain->TargetKills = 2;
                Brain->bQuestPhase = true;
                Brain->bLootPhase = true;
                Brain->bHarvestPhase = true;
                Brain->GlobalTimeoutSec = 600.0f;
            }
            else if (Scenario.Equals(TEXT("live_skills"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bSkillsPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_equip"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bEquipPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_level"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bLevelPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_titles_emotes"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bTitleEmotePhase = true;
            }
            else if (Scenario.Equals(TEXT("live_char_create_delete"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bCharFlowPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_back_to_login"), ESearchCase::IgnoreCase))
            {
                Brain->TargetKills = 0;
                Brain->bBackToLoginPhase = true;
            }
            else if (Scenario.Equals(TEXT("live_travel_roundtrip"), ESearchCase::IgnoreCase))
            {
                // Login -> game -> login -> game. Leg 1 runs like back_to_login
                // but reports without quitting; the driver supervises leg 2.
                Brain->TargetKills = 0;
                Brain->bBackToLoginPhase = (RoundtripLeg == 1);
                Brain->bTravelRoundtripPhase = (RoundtripLeg == 1);
                Brain->bRoundtripVerifyOnly = (RoundtripLeg == 2);
                Brain->GlobalTimeoutSec = 420.0f;
            }
            if (bTravelRoundtrip)
            {
                // Supervisor mode: keep polling across the leg-1 -> leg-2
                // handoff instead of stopping (Stop would kill the poll
                // ticker and auth bindings leg 2 still needs).
                Step = ELiveStep::WaitBrainLeg1;
                StepStartTime = Now;
            }
            else
            {
                Stop();
            }
            Brain->Start(GameInstance, TrackedWorld);
            return;
        }
        if (Now - StepStartTime > WorldTimeoutSec)
        {
            Finish(false, TEXT("world-timeout"));
        }
        return;
    }
}

void ULiveBotDriver::HandleLoginResponse(bool bSuccess, const FString& Message)
{
    if (!bRunning || !bLoginSent || Step != ELiveStep::WaitAuth) { return; }
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("LIVEBOT login rejected: %s"), *Message);
        Finish(false, TEXT("login-rejected"));
        return;
    }

    // Authenticated: request the character list (same call the UI makes).
    if (UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager())
    {
        FClientDataStruct D;
        D.clientId = GameInstance->GetCurrentClientID();
        D.hash = GameInstance->GetCurrentClientHash();
        Auth->SendCharacterListRequest(D);
        bCharsRequested = true;
        Step = ELiveStep::WaitChars;
        StepStartTime = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Warning, TEXT("LIVEBOT authenticated, characters requested"));
    }
    else
    {
        Finish(false, TEXT("no-auth-manager"));
    }
}

void ULiveBotDriver::HandleCharacterList(const TArray<FLoginCharacterEntry>& Characters)
{
    if (!bRunning || Step != ELiveStep::WaitChars) { return; }
    if (Characters.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("LIVEBOT no characters (seed one first)"));
        Finish(false, TEXT("no-characters"));
        return;
    }

    // First character, same as a player pressing Play on row 0.
    int32 CharId = Characters[0].CharacterId;
    const TCHAR* CharName = *Characters[0].CharacterName;
    if (bTravelRoundtrip && RoundtripLeg == 2)
    {
        // Rejoin the SAME character as leg 1 when still listed.
        for (const FLoginCharacterEntry& E : Characters)
        {
            if (E.CharacterId == Leg1CharId)
            {
                CharId = E.CharacterId;
                CharName = *E.CharacterName;
                break;
            }
        }
    }
    else
    {
        Leg1CharId = CharId;
    }
    UE_LOG(LogTemp, Warning, TEXT("LIVEBOT joining character %d ('%s')"),
        CharId, CharName);
    GameInstance->SetCurrentCharacterID(CharId);
    GameInstance->JoinSelectedCharacterToGame();
    if (bTravelRoundtrip && RoundtripLeg == 2)
    {
        // P0.2 double-join coverage: the FJoinGuard inside
        // JoinSelectedCharacterToGame must swallow the duplicate.
        GameInstance->JoinSelectedCharacterToGame();
    }
    Step = ELiveStep::WaitWorld;
    StepStartTime = FPlatformTime::Seconds();
}

void ULiveBotDriver::Finish(bool bPass, const FString& Reason)
{
    if (!bRunning) { return; }
    Stop();
    UE_LOG(LogTemp, Warning, TEXT("LIVEBOT_RESULT %s %s reason=%s"),
        *Scenario, bPass ? TEXT("PASS") : TEXT("FAIL"), *Reason);
    QuitClient();
}

void ULiveBotDriver::QuitClient() const
{
    if (UWorld* World = CurrentWorld.Get())
    {
        if (GEngine)
        {
            GEngine->Exec(World, TEXT("quit"));
            return;
        }
    }
    // No world (mid-travel failure): hard exit; Shutdown still runs and sends
    // disconnects for authenticated sessions.
    FPlatformMisc::RequestExit(false);
}
