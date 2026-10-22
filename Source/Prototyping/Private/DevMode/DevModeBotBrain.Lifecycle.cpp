#include "DevMode/DevModeBotBrain.h"
#include "DevMode/DevModeConsoleCommands.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Services/LocalizationSubsystem.h"
#include "Audio/AudioManager.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "UI/UIManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HighResScreenshot.h"
#include "Misc/Parse.h"
#include "DevMode/BotBrainHelpers.h"

void UDevModeBotBrain::Start(UMyGameInstance* InGameInstance, UWorld* InWorld)
{
    if (bRunning) { return; }
    GameInstance = InGameInstance;
    UWorld* World = InWorld ? InWorld : (GameInstance ? GameInstance->GetWorld() : nullptr);
    UE_LOG(LogTemp, Warning, TEXT("%s world: %s (tearing=%d)"),
        TEXT("DEVMODE_BOT"), World ? *World->GetName() : TEXT("null"),
        World ? (World->bIsTearingDown ? 1 : 0) : -1);
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!GameInstance || !World || !Player || World->bIsTearingDown)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s_RESULT %s FAIL no-player-or-world"),
            *TombstonePrefix, *ScenarioName);
        return;
    }
    BrainWorld = World;

    int32 Alive = 0;
    for (TActorIterator<ABasicMOB> It(World); It; ++It)
    {
        if (!(*It)->GetMOBIsDead()) { ++Alive; }
    }
    // Death scenarios run with TargetKills=0 (no victims needed offline);
    // every other scenario needs its kill budget present up front.
    if (Alive < TargetKills)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s_RESULT %s FAIL need-%d-mobs-have-%d (run devmode.reloadmobs first)"),
            *TombstonePrefix, *ScenarioName, TargetKills, Alive);
        return;
    }

    // Offline: start from the dev spawn; physics settles while we walk (real
    // falling). Live: start where the character actually is.
    if (bTeleportToSpawn)
    {
        Player->SetActorLocation(FVector(0.0f, 0.0f, 90.0f));
    }
    RunStartPos = Player->GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT spawn at (%.0f,%.0f,%.0f)"),
        RunStartPos.X, RunStartPos.Y, RunStartPos.Z);
    // Deterministic golden frames: re-seed mobs to JSON spots (boot aggro
    // lets them cluster around the player otherwise), then hold every mob's
    // locomotion driver still (wanderers photobombing shots, chase geometry
    // varying run to run). Offline only (live mobs are server-authoritative).
    // Re-enabled in Stop so manual console sessions keep a living world.
    if (bFreezeMobs)
    {
        if (GameInstance && GameInstance->DevModeConsoleCommands)
        {
            GameInstance->DevModeConsoleCommands->ReloadMobsNow();
        }
        int32 Frozen = 0;
        for (TActorIterator<ABasicMOB> It(World); It; ++It)
        {
            if (ABasicMOB* Mob = *It)
            {
                if (UMOBMovementComponent* MC = Mob->MOBMovementComponent)
                {
                    MC->Deactivate();
                    ++Frozen;
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("%s froze %d mob(s)"), *TombstonePrefix, Frozen);
    }
    // Offline revive target: the run spawn. Live respawn point is
    // server-authoritative (asserted as alive + HP, not position).
    RespawnPoint = RunStartPos;
    // Spawned dead (stale server-side death from a previous crashed run):
    // revive first so every scenario starts alive. Death scenarios re-die
    // for real afterwards, so the proof still holds.
    if (Player->GetIsDead())
    {
        Step = EBotStep::RecoverRevive;
        RespawnLegStartTime = FPlatformTime::Seconds();
        if (bStubDeath)
        {
            ApplyStubRevive(Player);
        }
        else
        {
            Player->OnRespawnClicked();
            UE_LOG(LogTemp, Warning, TEXT("%s spawned dead — respawn requested (stale death recovery)"),
                *TombstonePrefix);
        }
    }

    TargetMob.Reset();
    Kills = 0;
    Failures = 0;
    RunStartSec = FPlatformTime::Seconds();
    bDropSocketDone = false;
    LastCloseWaitLogSec = 0.0;
    Odometer = 0.0f;
    LastMoveTime = FPlatformTime::Seconds();
    Step = EBotStep::ExpectGround;
    StepStartTime = FPlatformTime::Seconds();
    // Phase A baselines: loot asserts inv growth, revive returns here.
    InitialInvCount = CountInventoryItems();
    InitialInvQty = SumInventoryQuantities();
    LootDropUID = 9001 + (GBotLootRunIndex++ % 50);
    LootDropActor.Reset();
    bPickupRequested = false;
    bLastMoveBlocked = false;
    LastBlockingActor.Empty();
    ShopLegStartTime = 0.0;
    bShopRequestSent = false;
    bVendorShopOpened = false;
    bSellRequested = false;
    bRepairShopRequestSent = false;
    bRepairShopOpened = false;
    bRepairRequested = false;
    SellInstanceId = 0;
    SellStartQty = 0;
    GoldBeforeSell = 0;
    bBuyRequested = false;
    BuyItemId = 0;
    BuyPrice = 0;
    BuyQtyBefore = 0;
    GoldBeforeBuy = 0;
    RepairInstanceId = 0;
    RepairMaxDur = 0;
    GoldBeforeRepair = 0;
    ChatLegStartTime = 0.0;
    bChatSent = false;
    PeerChatText.Empty();
    PeerCloseSince = 0.0;
    bPeerMsgSeen = false;
    LastChatResendTime = 0.0;
    TradeLegStartTime = 0.0;
    TradePeerCharId = 0;
    bTradeInviteSent = false;
    bTradeOfferSent = false;
    bTradeConfirmSent = false;
    GoldBeforeTrade = 0;
    QuestLegStartTime = 0.0;
    bDialogueRequested = false;
    bChoiceSent = false;
    GoldBeforeReward = 0;
    DialogueClicks = 0;
    LastAdvanceNodeId = 0;
    LastDialogueRequestTime = 0.0;
    LastInvResyncTime = 0.0;
    CharLegStartTime = 0.0;
    bSkillShopOpened = false;
    bLearnRequested = false;
    bCastSent = false;
    CastSentTime = 0.0;
    CastInRangeSince = 0.0;
    CombatCheckTime = 0.0;
    CombatCheckHp = 0;
    bBuffSent = false;
    bCastSteerInit = false;
    CastTargetUid = 0;
    MobHpBefore = 0;
    ManaBefore = 0;
    bDamageSeen = false;
    bCooldownSeen = false;
    bManaSeen = false;
    EquipInstanceId = 0;
    EquipItemSlug.Empty();
    StartLevel = 0;
    StartExp = 0;
    bTitleSent = false;
    bEmoteSent = false;
    bShopReqSent = false;
    bUnequipSent = false;
    LastLearnSlug.Empty();
    bWasPreEquipped = false;
    bRestorePass = false;
    FlowLegStartTime = 0.0;
    SavedLocale.Empty();
    MusicStage = 0;
    ZoomStage = 0;
    bListReqSent = false;
    bCreateSent = false;
    bDeleteSent = false;
    CharName.Empty();
    NewCharId = 0;
    NameLetterIdx = 0;
    CharListCache.Empty();
    CreateError.Empty();
    DeleteError.Empty();
    bLoginTravelSent = false;
    CorpseUID = 0;
    bHarvestRequested = false;
    bInspectRequested = false;
    bDeathAsserted = false;
    bRespawnRequested = false;
    bDeathWatchInit = false;
    DeathWatchHP = 0;
    LastDamageTime = 0.0;
    ExcludedKillerUIDs.Empty();
    ChaseCheckTime = 0.0;
    ChaseCheckDist = 0.0f;
    ChaseTargetUID = 0;
    bProvoked = false;
    ProvokeTime = 0.0;
    LootLegStartTime = 0.0;
    DeathLegStartTime = 0.0;
    RespawnLegStartTime = 0.0;
    ReviveSeenTime = 0.0;
    bWaypointInit = false;
    WaypointCheckTime = 0.0;
    WaypointCheckDist = 0.0f;
    // Deterministic locomotion proof: waypoint 150u from spawn TOWARD the
    // nearest mob (same heading as the coming chase — never into a blind
    // wall, unlike a fixed offset). Only physically walked distance counts.
    FVector ToMob = FVector(300.0f, 0.0f, 0.0f);
    if (ABasicMOB* FirstMob = FindNearestAliveMob())
    {
        ToMob = FirstMob->GetActorLocation() - RunStartPos;
        ToMob.Z = 0.0f;
        if (ToMob.Size() < 1.0f) { ToMob = FVector(300.0f, 0.0f, 0.0f); }
        ToMob.Normalize();
        ToMob *= 150.0f;
    }
    Waypoint = RunStartPos + ToMob;
    Waypoint.Z = RunStartPos.Z;
    OdometerAnchor = RunStartPos;
    LastChasePos = RunStartPos;
    LastChaseProgressTime = StepStartTime;
    bRunning = true;

    World->GetTimerManager().SetTimer(BrainTimerHandle, this, &UDevModeBotBrain::BrainTick, 0.1f, true);
    World->GetTimerManager().SetTimer(GlobalTimeoutHandle, this, &UDevModeBotBrain::OnGlobalTimeout, GlobalTimeoutSec, false);
    // Per-frame input pump (see SteerTick): intent published by SteerToward,
    // applied every frame so speed is fps-independent.
    bSteerIntent = false;
    LastSteerTime = 0.0;
    FTSTicker::GetCoreTicker().RemoveTicker(SteerTickerHandle);
    SteerTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UDevModeBotBrain::SteerTick), 0.0f);

    UE_LOG(LogTemp, Warning, TEXT("%s_BEGIN %s target-kills=%d"), *TombstonePrefix, *ScenarioName, TargetKills);
}

void UDevModeBotBrain::Stop()
{
    if (UWorld* World = BrainWorld.Get())
    {
        World->GetTimerManager().ClearTimer(BrainTimerHandle);
        World->GetTimerManager().ClearTimer(GlobalTimeoutHandle);
    }
    else
    {
        BrainTimerHandle.Invalidate();
        GlobalTimeoutHandle.Invalidate();
    }
    // Phase E legs that outlive world timers (delegates + core ticker).
    if (GameInstance && GameInstance->GetAuthenticationManager())
    {
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        Auth->OnCharacterListReceived.RemoveDynamic(this, &UDevModeBotBrain::HandleCharListReceived);
        Auth->OnCreateCharacterResponse.RemoveDynamic(this, &UDevModeBotBrain::HandleCreateResponse);
        Auth->OnDeleteCharacterResponse.RemoveDynamic(this, &UDevModeBotBrain::HandleDeleteResponse);
    }
    if (PostLoadMapLoginHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapLoginHandle);
        PostLoadMapLoginHandle.Reset();
    }
    FTSTicker::GetCoreTicker().RemoveTicker(LoginWatchdogHandle);
    LoginWatchdogHandle.Reset();
    FTSTicker::GetCoreTicker().RemoveTicker(SteerTickerHandle);
    SteerTickerHandle.Reset();
    bSteerIntent = false;
    // Unfreeze mob locomotion (see Start): manual console sessions keep
    // a living world after the brain stops.
    if (BrainWorld.IsValid())
    {
        for (TActorIterator<ABasicMOB> It(BrainWorld.Get()); It; ++It)
        {
            if (ABasicMOB* Mob = *It)
            {
                if (UMOBMovementComponent* MC = Mob->MOBMovementComponent)
                {
                    MC->Activate();
                }
            }
        }
    }
    bRunning = false;
    TargetMob.Reset();
}

void UDevModeBotBrain::OnGlobalTimeout()
{
    if (!bRunning) { return; }
    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT global timeout %.0fs — aborting"), (double)GlobalTimeoutSec);
    Finish(false, TEXT("global-timeout"));
}

ABasicMOB* UDevModeBotBrain::FindNearestAliveMob() const
{    UWorld* World = BrainWorld.Get();
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!World || !Player) { return nullptr; }

    ABasicMOB* Best = nullptr;
    float BestDist = FLT_MAX;
    const FVector At = Player->GetActorLocation();
    for (TActorIterator<ABasicMOB> It(World); It; ++It)
    {
        ABasicMOB* Mob = *It;
        if (!IsValid(Mob) || Mob->GetMOBIsDead()) { continue; }
        const float D = Dist2D(At, Mob->GetActorLocation());
        if (D < BestDist) { BestDist = D; Best = Mob; }
    }
    return Best;
}

ABasicMOB* UDevModeBotBrain::FindNearestAliveMobExcluding(const TSet<int32>& ExcludeUIDs) const
{
    UWorld* World = BrainWorld.Get();
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!World || !Player) { return nullptr; }

    ABasicMOB* Best = nullptr;
    float BestDist = FLT_MAX;
    const FVector At = Player->GetActorLocation();
    for (TActorIterator<ABasicMOB> It(World); It; ++It)
    {
        ABasicMOB* Mob = *It;
        if (!IsValid(Mob) || Mob->GetMOBIsDead()) { continue; }
        if (ExcludeUIDs.Contains(FCString::Atoi(*Mob->GetMOBUId()))) { continue; }
        const float D = Dist2D(At, Mob->GetActorLocation());
        if (D < BestDist) { BestDist = D; Best = Mob; }
    }
    return Best;
}

