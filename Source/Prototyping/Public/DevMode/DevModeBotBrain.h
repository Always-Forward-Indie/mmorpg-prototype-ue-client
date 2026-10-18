#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Authentication/LoginFlowTypes.h"
#include "DevModeBotBrain.generated.h"

class UMyGameInstance;
class ABasicPlayer;
class ABasicMOB;

// ============================================================================
// UDevModeBotBrain — L1 offline action bot (no servers).
//
// Drives the REAL local player through REAL game code: CharacterMovement
// steering via AddMovementInput, target lock via SetLockedTarget, the attack
// loop via OnAttackInput (approach/face/cooldown path). Mob HP is
// server-authoritative, so offline the killing blow is applied through the
// same SetMOBCurrentHealth+Die path as devmode.setmobhp — an honest stub of
// the server hit, logged as BOT_HIT_STUB. Everything else is real.
//
// Lifecycle: Start() teleports to spawn and begins patrol; BrainTick runs on
// the run world's TimerManager (0.1s). Every step has a timeout and there is a
// global timeout — the bot always ends with DEVMODE_BOT_RESULT PASS/FAIL and
// quits the client, never hangs CI. Held by a UPROPERTY member on the
// GameInstance (LoadMap force-collects unreferenced UObjects).
// Tombstone for log scan: "DEVMODE_BOT_RESULT <scenario> <PASS|FAIL> ...".
// ============================================================================
UCLASS()
class PROTOTYPING_API UDevModeBotBrain : public UObject
{
    GENERATED_BODY()

public:
    // Begins the run. World is passed EXPLICITLY (never GameInstance->
    // GetWorld() here): across travels the GI world context may be stale
    // (dead TimerManager = silently dead bot). Pass nullptr only on flows
    // with no travel after Start (DevMode), where GI->GetWorld() is used.
    void Start(UMyGameInstance* InGameInstance, UWorld* InWorld = nullptr);

    // Aborts the run (clears timers). Safe to call when not running.
    void Stop();

    bool IsRunning() const { return bRunning; }

    // ---- Live mode (set before Start; defaults = offline DevMode) ----
    // Tombstone identity.
    FString ScenarioName = TEXT("combat_patrol");
    FString TombstonePrefix = TEXT("DEVMODE_BOT");
    // Offline: teleport to the dev spawn at start. Live: start where the
    // character actually spawned (no godmode jumps on a live run).
    bool bTeleportToSpawn = true;
    // Offline: freeze all mob locomotion at start (deterministic golden
    // frames — no wanderers photobombing, no chase-geometry variance).
    // Live: never (server-authoritative mobs must keep moving).
    bool bFreezeMobs = false;
    // Offline: mob HP is server-authoritative, so the killing blow is an
    // honest stub via setmobhp path. Live: wait for the real server hit.
    bool bStubHits = true;
    // Quit the client when done (auto-run paths). Manual runs stay in world.
    bool bAutoQuitAtFinish = false;
    // Live only: give up waiting for a real server-confirmed kill after this.
    float CombatKillTimeoutSec = 120.0f;
    // Rendezvous mode (live_cross_visibility): after the waypoint leg, walk
    // to RendezvousPoint and assert a remote player actor is visible instead
    // of fighting. Default = village courtyard by Varan (real ground, town
    // = no aggro; proven by the vendor runs). The old starter value sat
    // over the void (bots fell to Z -500k, breaking local chat via the
    // server Z tolerance); override before Start if needed.
    bool bRendezvousMode = false;
    FVector RendezvousPoint = FVector(700.0f, -3200.0f, 200.0f);
    // Budget for reaching rendezvous + seeing the peer (both walk + spawn
    // propagation). Fails loudly on expiry, never hangs.
    float RendezvousTimeoutSec = 120.0f;
    // Live only: steer with swept direct movement instead of AddMovementInput.
    // Rationale (measured): on live servers the CharacterMovement input
    // pipeline barely converts (odometer ~2u/s at MaxWalkSpeed 320, cause
    // still open), while offline the same calls walk full speed. Swept moves
    // still test geometry/collision/arrival/combat/server-sync honestly;
    // CharacterMovement itself is proven offline (odometer 243u) and by
    // manual play. Locomotion proof = odometer either way.
    bool bDirectMove = false;
    // ---- Phase A: loot + death/respawn legs (set before Start) ----
    // loot_pickup: after TargetKills kills, walk to the last corpse, pick up
    // the drop and assert inv +1. Offline the drop spawn is stubbed through
    // the real ItemManager::SpawnDroppedItem path (logged BOT_LOOT_STUB);
    // live waits for the real server itemDrop and sends a real pickup.
    bool bLootPhase = false;
    // Offline: spawn the loot drop locally (no server offline). Live: wait
    // for the server drop and pick it up for real.
    bool bStubLoot = true;
    // death_respawn: prove death (death screen) + respawn (alive + HP + pos).
    // Offline death/revive are stubbed through the real SetDead(true/false)
    // path (logged BOT_DEATH_STUB / BOT_RESPAWN_STUB); live waits for real
    // mob damage and sends a real respawn request.
    bool bDeathPhase = false;
    // Offline: apply the killing blow via SetDead(true) (same path the
    // server-driven death takes). Live: stand down near a mob and wait for
    // real server damage (no attacks — bPassiveCombat).
    bool bStubDeath = true;
    // Live death leg: approach but never attack, let the mob hit back.
    bool bPassiveCombat = false;
    // Budgets for the new legs (seconds). Live fights/drops are slow.
    float LootTimeoutSec = 60.0f;
    float DeathTimeoutSec = 180.0f;
    float RespawnTimeoutSec = 60.0f;
    // ---- Phase B: economy (set before Start) ----
    // harvest_run: after the kill, run the real corpse-harvest progress
    // (server round-trip with duration) BEFORE inspect/pickup. Offline the
    // harvest completion is stubbed via the real SetHarvested path
    // (logged BOT_HARVEST_STUB). live_harvest_run proves the progress bar.
    bool bHarvestPhase = false;
    bool bStubHarvest = true;
    // vendor_repair (LIVE ONLY — no server offline): walk to the vendor,
    // open the shop, sell one junk stack (+gold), open the repair shop,
    // repair a damaged durable (durability=max, gold-). Polling-only, no
    // delegate bindings; inventory re-requested explicitly for determinism.
    bool bVendorPhase = false;
    int32 VendorNpcId = 1;
    FVector VendorPos = FVector(585.0f, -3300.0f, 200.0f);
    float ShopTimeoutSec = 60.0f;
    float HarvestTimeoutSec = 90.0f;
    // ---- Phase C: social + quests (set before Start) ----
    // Chat rides on cross_visibility: after peer-visible, both clients send
    // one "local" message and assert its arrival in the peer log + the peer
    // actor bubble. Needs TWO clients on the same scenario.
    bool bChatPhase = false;
    float ChatTimeoutSec = 60.0f;
    // quest_turnin (LIVE ONLY): walk to the quest NPC, accept via dialogue
    // (edge with quest preview), kill + harvest + loot the objective with
    // the Phase A/B machinery, walk back, turn in via dialogue (edge with
    // turn-in preview), assert journal + reward (gold + item).
    bool bQuestPhase = false;
    int32 QuestId = 9000;
    int32 QuestNpcId = 9000;
    FVector QuestNpcPos = FVector(700.0f, -3200.0f, 200.0f);
    float QuestTimeoutSec = 120.0f;
    // ---- Phase D: character (set before Start, all LIVE ONLY) ----
    // live_skills: trainer shop -> learn power_slash + battle_cry ->
    // cast damage skill (mob HP + cooldown + mana) -> cast self buff
    // (activeEffects). Needs a boosted bot (dev_learn.sql: level 5 + SP).
    bool bSkillsPhase = false;
    int32 SkillNpcId = 3;
    FVector SkillNpcPos = FVector(-634.0f, 2160.0f, 200.0f);
    // live_equip: equip main_hand (slot + visual mesh) -> unequip.
    bool bEquipPhase = false;
    // live_level: XP granted externally mid-run (admin grant_xp); poll
    // the level-up through ExperienceManager.
    bool bLevelPhase = false;
    // live_titles_emotes: equip earned title -> play emote (own actor).
    bool bTitleEmotePhase = false;
    float CharTimeoutSec = 90.0f;
    // ---- Phase E: flows + periphery ----
    // bot_periphery (OFFLINE): locale RU/EN toggle, music zone switch,
    // camera zoom in/out/clamp. No server needed.
    bool bPeripheryPhase = false;
    // live_char_create_delete: list -> create (unique letter name) ->
    // list contains -> delete -> list clean.
    bool bCharFlowPhase = false;
    // live_back_to_login: return to login, assert map, tombstone + quit
    // from the PostLoadMap callback (world timers die at travel).
    bool bBackToLoginPhase = false;
    // Roundtrip (live_travel_roundtrip): leg 1 reports at login without
    // quitting; leg 2 rejoins and a verify-only brain asserts the spawn.
    bool bTravelRoundtripPhase = false;
    bool bRoundtripLeg1Done = false;
    bool bRoundtripVerifyOnly = false;
    float FlowTimeoutSec = 60.0f;
    // Global run budget (seconds). Live runs set this higher (slow real fights).
    float GlobalTimeoutSec = 120.0f;
    // Kill budget for the combat legs. Scenario profiles (console + live
    // driver) set this before Start: 2 patrol, 1 loot, 0 death.
    int32 TargetKills = 2;
    // live_reconnect: drop the chunk socket once mid-run (self-inflicted via
    // UNetworkManager::DropChunkSocketForTest), then prove the run recovers:
    // the kill counter resets at the drop, so PASS needs TargetKills fresh
    // kills strictly after the rejoin. Default off — offline runs never drop.
    // The drop fires after the first confirmed kill (the fight is real and
    // the recovery is proven on a second one), or at DropSocketMaxWaitSec
    // as a fallback so a bad spot fails loud instead of hanging.
    bool bDropSocketMidRun = false;
    float DropSocketAfterSec = 15.0f;
    float DropSocketMaxWaitSec = 90.0f;
    // live_reconnect: only chase mobs within this radius. The scenario
    // proves recovery on a guaranteed-nearby fox (topped up externally),
    // never a cross-map chase into a mesh wall. Gated to live_reconnect.
    float CloseTargetRadius = 800.0f;
    // Patient wait for a close target before giving up (covers spawn lag).
    float NoCloseTargetTimeoutSec = 240.0f;

private:
    enum class EBotStep : uint8
    {
        // Loud pre-flight: the spawn must have ground under it (void falls
        // used to pass every 2D assert silently). Fails in 5s if never grounded.
        ExpectGround,
        GotoWaypoint,
        GotoTarget,
        Combat,
        AssertKill,
        GotoRendezvous,
        AssertVisible,
        GotoCorpse,
        LootPickup,
        ExpectDeath,
        AssertDead,
        RespawnWait,
        RecoverRevive,
        VendorWalk,
        VendorSell,
        VendorBuy,
        VendorRepair,
        ChatExchange,
        QuestWalk,
        QuestAccept,
        QuestTurnin,
        SkillWalk,
        SkillLearn,
        SkillCast,
        SkillBuff,
        EquipOn,
        EquipOff,
        LevelWait,
        TitleLeg,
        EmoteLeg,
        LocaleLeg,
        MusicLeg,
        CameraLeg,
        CharList,
        CharCreate,
        CharDelete,
        BackToLogin,
    };

    UFUNCTION()
    void BrainTick();
    UFUNCTION()
    void OnGlobalTimeout();
    // Nearest living mob actor, or nullptr.
    ABasicMOB* FindNearestAliveMob() const;
    // Nearest living mob outside the exclusion set (death leg rotates away
    // from dummies and unreachable picks).
    ABasicMOB* FindNearestAliveMobExcluding(const TSet<int32>& ExcludeUIDs) const;
    // Shared steering: real AddMovementInput toward Dest, odometer accounting
    // (teleport jumps never count), stuck/teleport fallback. Returns true on
    // arrival within ArriveRadius. Label names the leg in fallback logs.
    bool SteerToward(ABasicPlayer* Player, const FVector& Dest, float ArriveRadius, double Now, const TCHAR* Label);
    // Per-frame movement pump: BrainTick runs at 10Hz but CharacterMovement
    // consumes AddMovementInput EVERY rendered frame, so input applied only
    // in BrainTick moves the pawn at (frame_rate/10Hz) duty — ~20u/s at
    // 300fps instead of walk speed (fps-dependent crawl, leg timeouts fire
    // on fast machines). The state machine publishes intent in SteerToward;
    // this core ticker (world-independent, survives travel) reapplies the
    // input each frame from the live position until the intent goes stale.
    bool SteerTick(float);
    // Per-step legs of the BrainTick state machine (one method per EBotStep,
    // defined in DevModeBotBrain.Tick*.cpp). Each leg takes the same tick
    // context the inline block used (World/Player/Now/PlayerAt) and returns
    // true when the original block returned from BrainTick (tick ends),
    // false on fall-through so the dispatcher keeps scanning — control flow
    // is identical to the pre-split inline chain.
    bool Tick_ExpectGround(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_AssertKill(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_GotoWaypoint(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_GotoRendezvous(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_AssertVisible(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_GotoCorpse(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_LootPickup(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_ExpectDeath(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_AssertDead(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_RespawnWait(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_RecoverRevive(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_VendorWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_VendorSell(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_VendorBuy(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_VendorRepair(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_ChatExchange(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_QuestWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_QuestAccept(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_QuestTurnin(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_SkillWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_SkillLearn(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_SkillCast(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_SkillBuff(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_EquipOn(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_EquipOff(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_LevelWait(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_TitleLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_EmoteLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_LocaleLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_MusicLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_CameraLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_CharList(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_CharCreate(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_CharDelete(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    bool Tick_BackToLogin(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt);
    // Tick_GotoTarget/Tick_Combat additionally take the live-target
    // preamble the dispatcher computes between BackToLogin and their legs
    // (Mob/MobAt/Range/Dist) — same values the inline tail produced.
    bool Tick_GotoTarget(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt, ABasicMOB* Mob, const FVector& MobAt, float Range, float Dist);
    bool Tick_Combat(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt, ABasicMOB* Mob, const FVector& MobAt, float Range, float Dist);
    // Grounded reposition near Dest for the stuck fallback: traces down at a
    // few lateral candidates and takes the first with ground (never lands
    // inside rock). Falls back to raw Dest+100Z if nothing hits.
    FVector FindTeleportSpot(ABasicPlayer* Player, const FVector& Dest) const;
    // Logs the tombstone, stops timers, quits the client (auto-run path).
    void Finish(bool bPass, const FString& Reason);
    // Local assert in DEVMODE_ASSERT format (same as devmode.expect).
    void BotAssert(bool bPass, const FString& What, int32 Actual, int32 Expected);
    // live_reconnect: one-shot mid-run socket drop (see bDropSocketMidRun).
    void MaybeDropSocketMidRun(double Now);
    // Checkpoint screenshot for watchable runs (-devmode-shots flag only;
    // headless runs skip). Named DevBot_<scenario>_<tag>.
    void BotShot(const FString& Tag) const;
    // Phase A helpers. Stubs go through the same game code as the live path
    // (spawn/resolve/death/revive) and are logged BOT_*_STUB; everything else
    // about the leg (movement, asserts, timeouts) is identical in both modes.
    class ADroppedItemActor* FindNearestDroppedItem(float MaxDistance) const;
    // Live loot corpse: our victim if its actor still stands, else the
    // nearest (harvestable or harvested) corpse — we stand on our kill.
    class ABasicMOB* ResolveCorpseActor() const;
    bool SpawnStubDrop(ABasicPlayer* Player);
    void ResolveStubLoot(ABasicPlayer* Player);
    void ApplyStubDeath(ABasicPlayer* Player);
    void ApplyStubRevive(ABasicPlayer* Player);
    int32 CountInventoryItems() const;
    int32 SumInventoryQuantities() const;
    bool IsDeathScreenVisible() const;
    // Phase B helpers (polling-only economy legs).
    int32 GetGold() const;
    // First stackable, non-equipped, non-durable, non-quest row (sell junk).
    int32 FindSellableInstanceId(int32& OutQty) const;
    // First damaged durable row (repair target).
    int32 FindDamagedInstanceId(int32& OutMaxDur) const;
    int32 GetQtyByInstance(int32 InstanceId) const;
    int32 GetDurabilityByInstance(int32 InstanceId) const;
    // Phase C helpers.
    bool HasItemTemplate(int32 TemplateItemId) const;
    // Peer actor of a rendezvous run (any SpawnedPlayers entry that is not
    // this client's own), or nullptr.
    class ABasicPlayer* FindPeerActor() const;
    // Phase C dialogue walker (quest legs).
    bool EnsureDialogueOpen(int32 NpcId, double Now);
    // Goal edge (accept or turn-in preview, condition met), or 0.
    int32 FindGoalEdge(bool bWantTurnin) const;
    // First conditionMet non-farewell edge (continue/back hubs), or 0.
    int32 FindAdvanceEdge() const;
    // Quest loot checkpoint (called after each corpse resolve): all kills
    // looted -> turn in; otherwise hunt the next victim.
    void QuestLootDone(double Now);
    // Phase E auth-flow handlers (bound only during the char legs).
    UFUNCTION()
    void HandleCharListReceived(const TArray<FLoginCharacterEntry>& Characters);
    UFUNCTION()
    void HandleCreateResponse(bool bSuccess, const FString& Message, int32 CharacterId);
    UFUNCTION()
    void HandleDeleteResponse(bool bSuccess, const FString& Message, int32 CharacterId);
    // PostLoadMap callback for the back-to-login leg (world timers are
    // dead after travel — the tombstone fires here, not in BrainTick).
    void HandlePostLoadMapLogin(UWorld* LoadedWorld);
    // Phase D helpers.
    bool HasActiveEffect(const FString& Slug) const;
    int32 FindEquippableMainHand(FString& OutSlug) const;
    int32 GetProgressionLevel() const;
    int32 GetProgressionExp() const;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    // The world this run lives on (timers, iteration). Resolved ONCE at Start:
    // re-querying GameInstance->GetWorld() every tick is unsafe across travels
    // (stale context → dead TimerManager = silently dead bot).
    TWeakObjectPtr<UWorld> BrainWorld;

    FTimerHandle BrainTimerHandle;
    FTimerHandle GlobalTimeoutHandle;
    // Back-to-login leg tracking (survives the travel that kills the
    // world timers above).
    FDelegateHandle PostLoadMapLoginHandle;
    FTSTicker::FDelegateHandle LoginWatchdogHandle;
    // Current steering intent (published by SteerToward at 10Hz, consumed
    // per-frame by SteerTick). Stale intents (>0.3s) stop applying so no
    // input leaks across legs or travels.
    FTSTicker::FDelegateHandle SteerTickerHandle;
    FVector SteerDest = FVector::ZeroVector;
    double LastSteerTime = 0.0;
    bool bSteerIntent = false;
    // Parked BotShot name (same-tick bursts share one screenshot pipe).
    mutable FString PendingShotName;

    EBotStep Step = EBotStep::GotoWaypoint;
    TWeakObjectPtr<ABasicMOB> TargetMob;
    FVector RunStartPos = FVector::ZeroVector;
    // Deterministic locomotion proof: first walk to a waypoint 300u away.
    // Only physically walked distance counts (teleport fallback jumps never
    // add to the odometer), so a pass always means real movement.
    FVector Waypoint = FVector::ZeroVector;
    FVector OdometerAnchor = FVector::ZeroVector;
    float Odometer = 0.0f;
    double LastMoveTime = 0.0;
    double StepStartTime = 0.0;
    double CombatStartTime = 0.0;
    FVector LastChasePos = FVector::ZeroVector;
    double LastChaseProgressTime = 0.0;
    int32 Kills = 0;
    int32 Failures = 0;
    bool bRunning = false;
    // live_reconnect bookkeeping: run clock for the drop delay + one-shot latch.
    double RunStartSec = 0.0;
    bool bDropSocketDone = false;
    // live_reconnect: throttle for the "waiting for close target" notice.
    double LastCloseWaitLogSec = 0.0;
    bool bAttackStarted = false;
    // ---- Phase A state ----
    // Where the last counted kill died (loot leg walks back here).
    FVector LastKillLocation = FVector::ZeroVector;
    // Offline revive target (= run spawn). Live respawn point is server-side.
    FVector RespawnPoint = FVector::ZeroVector;
    // Inventory baseline for the loot assert (count AND quantity: server
    // snapshots may merge stacks, so either growing counts as proof).
    int32 InitialInvCount = 0;
    int32 InitialInvQty = 0;
    // Stub drop identity (dev range, never collides with seeded ids).
    int32 LootDropUID = 9001;
    int32 LootItemTemplateId = 201;
    // Live loot: the server drop we are walking to / picking up.
    TWeakObjectPtr<class ADroppedItemActor> LootDropActor;
    bool bPickupRequested = false;
    // Live loot follows the real game flow: harvest the victim's corpse,
    // inspect its loot, pick it all up (world itemDrop packets on the dev
    // стенд carry empty item lists — corpse loot is the loot path).
    int32 CorpseUID = 0;
    bool bHarvestRequested = false;
    bool bInspectRequested = false;
    // Death leg guards (the top-of-tick dead check redirects here).
    bool bDeathAsserted = false;
    bool bRespawnRequested = false;
    // Live death watch: rotate away from killers that never aggro. Baseline
    // HP + last-progress time; a killer dealing zero damage in
    // KillerDummySec is excluded and the bot walks to the next one.
    bool bDeathWatchInit = false;
    int32 DeathWatchHP = 0;
    double LastDamageTime = 0.0;
    // Live death rotation: killers that deal no damage (dummies) or cannot
    // be reached (server reverts off-pace moves) are excluded by UID and
    // the bot walks to the next one.
    TSet<int32> ExcludedKillerUIDs;
    double ChaseCheckTime = 0.0;
    float ChaseCheckDist = 0.0f;
    // Combat chase target the progress baseline belongs to.
    int32 ChaseTargetUID = 0;
    // Last swept-step collision (live diagnostics for STUCK lines).
    bool bLastMoveBlocked = false;
    FString LastBlockingActor;
    // Chat settle: time since the peer entered local radius (server
    // positions + replication lag the client, so speak only after holding
    // range — never on a transient tick).
    double PeerCloseSince = 0.0;
    // ---- Phase B state ----
    double ShopLegStartTime = 0.0;
    bool bShopRequestSent = false;
    bool bVendorShopOpened = false;
    bool bSellRequested = false;
    bool bRepairShopRequestSent = false;
    bool bRepairShopOpened = false;
    bool bRepairRequested = false;
    int32 SellInstanceId = 0;
    int32 SellStartQty = 0;
    int32 GoldBeforeSell = 0;
    bool bBuyRequested = false;
    int32 BuyItemId = 0;
    int32 BuyPrice = 0;
    int32 BuyQtyBefore = 0;
    int32 GoldBeforeBuy = 0;
    int32 RepairInstanceId = 0;
    int32 RepairMaxDur = 0;
    int32 GoldBeforeRepair = 0;
    // ---- Phase C state ----
    double ChatLegStartTime = 0.0;
    bool bChatSent = false;
    FString PeerChatText;
    double QuestLegStartTime = 0.0;
    bool bDialogueRequested = false;
    bool bChoiceSent = false;
    int32 GoldBeforeReward = 0;
    // Dialogue walker: goal edge first, else advance through continue/back
    // hubs (never farewell). Click budget + node tracking prevent loops.
    int32 DialogueClicks = 0;
    int32 LastAdvanceNodeId = 0;
    double LastDialogueRequestTime = 0.0;
    // ---- Phase D state ----
    double CharLegStartTime = 0.0;
    bool bSkillShopOpened = false;
    bool bLearnRequested = false;
    bool bCastSent = false;
    bool bBuffSent = false;
    int32 CastTargetUid = 0;
    int32 MobHpBefore = 0;
    int32 ManaBefore = 0;
    // Skill-hit latches (damage, cooldown and mana land at different
    // times: cooldown arms on initiation, mana on stats_update, damage on
    // the deferred result flush — up to ~12s apart).
    bool bDamageSeen = false;
    bool bCooldownSeen = false;
    bool bManaSeen = false;
    int32 EquipInstanceId = 0;
    FString EquipSlotSlug = TEXT("main_hand");
    FString EquipItemSlug;
    int32 StartLevel = 0;
    int32 StartExp = 0;
    bool bTitleSent = false;
    bool bEmoteSent = false;
    // Owned emote actually played (first row of the server player_emotes
    // snapshot — never assume a slug is unlocked).
    FString EmoteSlug;
    // ---- Phase E state ----
    double FlowLegStartTime = 0.0;
    FString SavedLocale;
    int32 MusicStage = 0;
    int32 ZoomStage = 0;
    bool bListReqSent = false;
    bool bCreateSent = false;
    bool bDeleteSent = false;
    FString CharName;
    int32 NewCharId = 0;
    int32 NameLetterIdx = 0;
    TArray<FLoginCharacterEntry> CharListCache;
    FString CreateError;
    FString DeleteError;
    bool bLoginTravelSent = false;
    bool bShopReqSent = false;
    bool bUnequipSent = false;
    FString LastLearnSlug;
    // Equip round-trip is idempotent: a pre-equipped slot is verified,
    // unequipped, then restored.
    bool bWasPreEquipped = false;
    bool bRestorePass = false;
    // Live death pull: mobs don't aggro a passive player, so the bot lands
    // ONE honest hit (real server damage, starts retaliation) and then
    // stops the loop via StopAutoAttack. The killing blows stay server-side.
    bool bProvoked = false;
    double ProvokeTime = 0.0;
    // Leg budgets measured from leg entry (SteerToward rewrites
    // StepStartTime on teleport fallback, so legs keep their own clocks).
    double LootLegStartTime = 0.0;
    double DeathLegStartTime = 0.0;
    double RespawnLegStartTime = 0.0;
    // First tick the player is seen alive after respawn. The HP snapshot
    // (stats_update) lands a beat AFTER the dead flag clears live — the
    // HP assert waits a grace period instead of reading stale zero.
    double ReviveSeenTime = 0.0;

    // Step budgets (seconds). Chasing across the small dev area takes seconds;
    // anything beyond the budget teleports (logged fallback) or fails.
    // GotoTimeoutSec must stay ABOVE ChaseStallSec: the stall safeguard
    // (exclude the pick, repick, fresh clock) samples every ChaseStallSec,
    // so a shorter leg budget would time out first and the safeguard could
    // never fire (seen live: 25s leg vs 30s stall window — obstacle slides
    // always failed instead of repicking). Clean 600u chase takes ~6s;
    // 45s leaves headroom to round obstacles while the 120s global watchdog
    // still bounds the scenario.
    static constexpr float GotoTimeoutSec = 45.0f;
    static constexpr float StuckTimeoutSec = 5.0f;
    static constexpr float CombatTimeoutSec = 12.0f;
    static constexpr float ArrivalFraction = 0.8f;
    // A live killer that deals no damage for this long is a dummy (wrong
    // faction, leashed, or geometry-separated) — exclude and move on.
    static constexpr float KillerDummySec = 45.0f;
    // A live chase making no distance gain for this long is stuck behind
    // geometry the server won't let us cross — exclude and move on.
    static constexpr float ChaseStallSec = 30.0f;
    static constexpr float ChaseMinGain = 200.0f;
};
