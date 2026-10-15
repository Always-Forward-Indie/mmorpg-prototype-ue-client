#include "DevMode/DevModeBotBrain.h"
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
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "UI/UIManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HighResScreenshot.h"
#include "Misc/Parse.h"

namespace
{
float Dist2D(const FVector& A, const FVector& B)
{
    const float DX = A.X - B.X;
    const float DY = A.Y - B.Y;
    return FMath::Sqrt(DX * DX + DY * DY);
}

// Stub-drop UID rotator: ResolveStubLoot destroys the actor but the
// ItemManager map keeps the entry for the session, so each run mints a
// fresh dev-range UID (9001+). Never collides with seeded ids (101..301).
int32 GBotLootRunIndex = 0;
}

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
    InitialAlive = Alive;

    // Offline: start from the dev spawn; physics settles while we walk (real
    // falling). Live: start where the character actually is.
    if (bTeleportToSpawn)
    {
        Player->SetActorLocation(FVector(0.0f, 0.0f, 90.0f));
    }
    RunStartPos = Player->GetActorLocation();
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
    Odometer = 0.0f;
    LastMoveTime = FPlatformTime::Seconds();
    Step = EBotStep::GotoWaypoint;
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
    RepairInstanceId = 0;
    RepairMaxDur = 0;
    GoldBeforeRepair = 0;
    ChatLegStartTime = 0.0;
    bChatSent = false;
    PeerChatText.Empty();
    PeerCloseSince = 0.0;
    QuestLegStartTime = 0.0;
    bDialogueRequested = false;
    bChoiceSent = false;
    GoldBeforeReward = 0;
    DialogueClicks = 0;
    LastAdvanceNodeId = 0;
    LastDialogueRequestTime = 0.0;
    CharLegStartTime = 0.0;
    bSkillShopOpened = false;
    bLearnRequested = false;
    bCastSent = false;
    bBuffSent = false;
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

FVector UDevModeBotBrain::FindTeleportSpot(ABasicPlayer* Player, const FVector& Dest) const
{
    UWorld* World = BrainWorld.Get();
    if (!World || !Player)
    {
        return Dest + FVector(0.0f, 0.0f, 100.0f);
    }

    static const FVector Candidates[] = {
        FVector(-50.0f, 0.0f, 0.0f),
        FVector(50.0f, 0.0f, 0.0f),
        FVector(0.0f, -50.0f, 0.0f),
        FVector(0.0f, 50.0f, 0.0f),
    };

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Player);
    Params.bTraceComplex = false;
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

    for (const FVector& Off : Candidates)
    {
        const FVector XY(Dest.X + Off.X, Dest.Y + Off.Y, Dest.Z);
        const FVector Start = XY + FVector(0.0f, 0.0f, 500.0f);
        const FVector End = XY - FVector(0.0f, 0.0f, 2000.0f);
        FHitResult Hit;
        if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params))
        {
            return FVector(XY.X, XY.Y, Hit.Location.Z + 100.0f);
        }
    }
    return Dest + FVector(0.0f, 0.0f, 100.0f);
}

bool UDevModeBotBrain::SteerToward(ABasicPlayer* Player, const FVector& Dest, float ArriveRadius, double Now, const TCHAR* Label)
{
    if (!Player) { return true; }
    const FVector PlayerAt = Player->GetActorLocation();

    // Odometer counts physically walked distance only: teleport fallback jumps
    // update the anchor without adding, so a pass always means real walking.
    Odometer += Dist2D(PlayerAt, OdometerAnchor);
    OdometerAnchor = PlayerAt;

    if (Dist2D(PlayerAt, Dest) <= ArriveRadius) { return true; }

    if (bDirectMove)
    {
        // Swept kinematic step at the pawn's real MaxWalkSpeed (0.1s timer
        // quantum, clamped): deterministic, collision-respecting, no reliance
        // on the CharacterMovement input pipeline.
        const UCharacterMovementComponent* MC = Player->GetCharacterMovement();
        const float Speed = MC ? MC->MaxWalkSpeed : 300.0f;
        const double Now2 = FPlatformTime::Seconds();
        const float Dt = (float)FMath::Min(Now2 - LastMoveTime, 0.25);
        LastMoveTime = Now2;
        FVector Dir = Dest - PlayerAt;
        Dir.Z = 0.0f;
        if (Dir.Normalize())
        {
            FHitResult Hit;
            Player->AddActorWorldOffset(Dir * Speed * Dt, true, &Hit);
            bLastMoveBlocked = Hit.bBlockingHit;
            LastBlockingActor = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("none");
        }
    }
    else
    {
        // Real steering through CharacterMovementComponent (physics, collision).
        FVector Dir = Dest - PlayerAt;
        Dir.Z = 0.0f;
        if (!Dir.Normalize()) { return true; }
        Player->AddMovementInput(Dir, 1.0f);
    }

    // Stuck (no 50u progress in StuckTimeoutSec): offline teleports near the
    // destination (logged fallback); live keeps honest swept pace and logs
    // BOT_STUCK. Leg entry clocks (StepStartTime et al.) are NEVER touched
    // here — otherwise the leg timeouts below could never fire and a stuck
    // leg would idle until the global watchdog. Never a hang either way.
    if (Dist2D(PlayerAt, LastChasePos) > 50.0f)
    {
        LastChasePos = PlayerAt;
        LastChaseProgressTime = Now;
    }
    if (Now - LastChaseProgressTime > StuckTimeoutSec)
    {
        LastChaseProgressTime = Now;
        if (bDirectMove)
        {
            // Live: the chunk server reverts off-pace moves
            // (positionCorrection), so admin teleports never stick.
            LastChasePos = PlayerAt;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT_STUCK %s (dist %.0f, no teleport on live) from (%.0f,%.0f,%.0f) blocked=%d on='%s'"),
                Label, Dist2D(PlayerAt, Dest), PlayerAt.X, PlayerAt.Y, PlayerAt.Z,
                bLastMoveBlocked ? 1 : 0, *LastBlockingActor);
        }
        else
        {
        // Admin repositioning (accepted L1 practice): grounded spot inside
        // arrival range so the leg completes next tick. Odometer keeps only
        // really walked distance, so the locomotion proof stays honest.
        const FVector Near = FindTeleportSpot(Player, Dest);
        Player->SetActorLocation(Near);
        LastChasePos = Near;
        OdometerAnchor = Near;
        const FVector Vel = Player->GetVelocity();
        const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
        AController* PC = Player->GetController();
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT_FALLBACK teleport %s (dist %.0f) pawn='%s' ctrl='%s' locallycontrolled=%d maxwalk=%.0f compregistered=%d isother=%d from (%.0f,%.0f,%.0f) vel=%.0f mode=%d"),
            Label, Dist2D(PlayerAt, Dest), *Player->GetName(),
            PC ? *PC->GetName() : TEXT("null"),
            Player->IsLocallyControlled() ? 1 : 0,
            MoveComp ? MoveComp->MaxWalkSpeed : -1.0f,
            MoveComp ? (MoveComp->IsRegistered() ? 1 : 0) : -1,
            Player->GetIsOtherClient() ? 1 : 0,
            PlayerAt.X, PlayerAt.Y, PlayerAt.Z, Vel.Size(),
            MoveComp ? (int32)MoveComp->MovementMode : -1);
        if (MoveComp)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT_INPUT pendmm=%d lastmm=%d"),
                FMath::RoundToInt(MoveComp->GetPendingInputVector().Size() * 1000.0),
                FMath::RoundToInt(MoveComp->GetLastInputVector().Size() * 1000.0));
        }
        }
    }
    return false;
}

void UDevModeBotBrain::BrainTick()
{
    if (!bRunning) { return; }
    UWorld* World = BrainWorld.Get();
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!World || !Player || Player->IsActorBeingDestroyed())
    {
        Finish(false, TEXT("lost-world-or-player"));
        return;
    }

    const double Now = FPlatformTime::Seconds();

    // Death is fatal by default (death UI needs a human click) — but the
    // death_respawn scenario EXPECTS it: redirect into the assert leg
    // instead of failing. RespawnWait polls the revival itself.
    if (Player->GetIsDead())
    {
        if (!bDeathPhase)
        {
            Finish(false, TEXT("player-died"));
            return;
        }
        if (Step != EBotStep::AssertDead && Step != EBotStep::RespawnWait && Step != EBotStep::RecoverRevive)
        {
            Step = EBotStep::AssertDead;
            StepStartTime = Now;
        }
    }

    // (Re)acquire a living target whenever needed — except in AssertKill
    // (runs against the just-killed mob) and GotoWaypoint (needs no target).
    // Mid-combat death (server hit lands between ticks): do NOT repick —
    // fall through to AssertKill so the kill is counted, not skipped.
    if ((Step == EBotStep::GotoTarget || Step == EBotStep::Combat)
        && (!TargetMob.IsValid() || TargetMob->GetMOBIsDead()))
    {
        if (Step == EBotStep::Combat && TargetMob.IsValid())
        {
            Step = EBotStep::AssertKill;
        }
        else
        {
            // Exclusion set is death-mode-only in practice (empty for
            // combat runs) — shared helper keeps both honest.
            TargetMob = FindNearestAliveMobExcluding(ExcludedKillerUIDs);
            if (!TargetMob.IsValid())
            {
                Finish(Kills >= TargetKills, Kills >= TargetKills ? TEXT("no-targets-left") : TEXT("target-lost"));
                return;
            }
            Step = EBotStep::GotoTarget;
            StepStartTime = Now;
            LastChasePos = Player->GetActorLocation();
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Log, TEXT("DEVMODE_BOT target '%s' at (%.0f,%.0f)"),
                *TargetMob->GetMobName(), TargetMob->GetActorLocation().X, TargetMob->GetActorLocation().Y);
        }
    }

    const FVector PlayerAt = Player->GetActorLocation();

    if (Step == EBotStep::AssertKill)
    {
        ABasicMOB* DeadMob = TargetMob.Get();
        const bool bDead = !IsValid(DeadMob) || DeadMob->GetMOBIsDead();
        BotAssert(bDead, TEXT("mob-dead"), bDead ? 1 : 0, 1);

        int32 Alive = 0;
        for (TActorIterator<ABasicMOB> It(World); It; ++It)
        {
            if (!(*It)->GetMOBIsDead()) { ++Alive; }
        }
        // Kills must drain the living count one by one (started at InitialAlive).
        BotAssert(Alive == InitialAlive - Kills - 1, TEXT("alive-count"), Alive, InitialAlive - Kills - 1);

        // Real locomotion is proven by the waypoint leg (odometer), not here.
        BotAssert(bAttackStarted, TEXT("attack-loop-active"), bAttackStarted ? 1 : 0, 1);

        if (!bDead)
        {
            Finish(false, TEXT("kill-not-registered"));
            return;
        }
        if (IsValid(DeadMob))
        {
            LastKillLocation = DeadMob->GetActorLocation();
            CorpseUID = FCString::Atoi(*DeadMob->GetMOBUId());
        }
        ++Kills;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT kill %d/%d confirmed"), Kills, TargetKills);
        BotShot(FString::Printf(TEXT("kill%d"), Kills));

        if (Kills >= TargetKills)
        {
            // Loot leg walks back to the last corpse instead of quitting.
            if (bLootPhase)
            {
                Step = EBotStep::GotoCorpse;
                StepStartTime = Now;
                LootLegStartTime = Now;
                LastChasePos = PlayerAt;
                LastChaseProgressTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT kills done, walking to corpse (%.0f,%.0f)"),
                    LastKillLocation.X, LastKillLocation.Y);
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("all-kills-done") : TEXT("assert-failures"));
            return;
        }
        bAttackStarted = false;
        TargetMob.Reset();
        Step = EBotStep::GotoTarget;
        StepStartTime = Now;
        LastChasePos = PlayerAt;
        LastChaseProgressTime = Now;
        return;
    }

    if (Step == EBotStep::GotoWaypoint)
    {
        // Deterministic locomotion proof before any combat: walk the leg.
        // Arrival radius 60 on a 150u leg guarantees >=90u of real walking;
        // threshold 50 keeps margin. Teleport jumps never count (odometer).
        if (SteerToward(Player, Waypoint, 60.0f, Now, TEXT("waypoint")))
        {
            BotAssert(Odometer > 50.0f, TEXT("player-walked"), (int32)Odometer, 50);
            if (bSkillsPhase)
            {
                Step = EBotStep::SkillWalk;
                CharLegStartTime = Now;
            }
            else if (bEquipPhase)
            {
                Step = EBotStep::EquipOn;
                CharLegStartTime = Now;
            }
            else if (bLevelPhase)
            {
                Step = EBotStep::LevelWait;
                CharLegStartTime = Now;
                StartLevel = GetProgressionLevel();
                StartExp = GetProgressionExp();
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT level baseline %d exp %d, waiting for grant"),
                    StartLevel, StartExp);
            }
            else if (bTitleEmotePhase)
            {
                Step = EBotStep::TitleLeg;
                CharLegStartTime = Now;
            }
            else if (bQuestPhase)
            {
                // Quest chain: locomotion proven, walk to the quest NPC.
                Step = EBotStep::QuestWalk;
                QuestLegStartTime = Now;
            }
            else if (bVendorPhase)
            {
                // Economy run: locomotion proven, walk to the vendor next.
                Step = EBotStep::VendorWalk;
                ShopLegStartTime = Now;
            }
            else if (bDeathPhase)
            {
                // No combat in the death scenario: prove locomotion, then die.
                Step = EBotStep::ExpectDeath;
                DeathLegStartTime = Now;
            }
            else if (bRendezvousMode)
            {
                Step = EBotStep::GotoRendezvous;
            }
            else
            {
                Step = EBotStep::GotoTarget;
            }
            StepStartTime = Now;
            LastChasePos = Player->GetActorLocation();
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT waypoint reached (odometer %.0f)"), Odometer);
            BotShot(TEXT("waypoint"));
            return;
        }
        if (Now - StepStartTime > GotoTimeoutSec)
        {
            Finish(false, TEXT("waypoint-timeout"));
        }
        return;
    }

    if (Step == EBotStep::GotoRendezvous)
    {
        // Walk to the meeting point (server-tick honest pace via SteerToward),
        // then wait for the peer's remote actor. Arrival radius is generous:
        // visibility interest ranges are wide, precision doesn't matter.
        if (SteerToward(Player, RendezvousPoint, 150.0f, Now, TEXT("rendezvous")))
        {
            Step = EBotStep::AssertVisible;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at rendezvous, waiting for peer"));
            BotShot(TEXT("rendezvous"));
            return;
        }
        if (Now - StepStartTime > RendezvousTimeoutSec)
        {
            Finish(false, TEXT("rendezvous-timeout"));
        }
        return;
    }

    if (Step == EBotStep::AssertVisible)
    {
        // A remote player is visible iff SpawnedPlayers holds a live actor
        // under someone else's clientId (this client owns exactly one entry).
        // Assert exactly once (on success or timeout), not per poll.
        const int32 OwnId = GameInstance->GetCurrentClientID();
        bool bPeerVisible = false;
        for (const TPair<int32, ABasicPlayer*>& E : GameInstance->SpawnedPlayers)
        {
            if (E.Key != OwnId && IsValid(E.Value) && !E.Value->IsActorBeingDestroyed())
            {
                bPeerVisible = true;
                break;
            }
        }
        if (bPeerVisible)
        {
            BotAssert(true, TEXT("peer-visible"), 1, 1);
            if (bChatPhase)
            {
                // Social leg: stay at the rendezvous and talk next.
                Step = EBotStep::ChatExchange;
                ChatLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT peer visible, starting chat exchange"));
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("peer-visible") : TEXT("assert-failures"));
            return;
        }
        if (Now - StepStartTime > RendezvousTimeoutSec)
        {
            BotAssert(false, TEXT("peer-visible"), 0, 1);
            Finish(false, TEXT("peer-timeout"));
        }
        return;
    }

    if (Step == EBotStep::GotoCorpse)
    {
        // Walk back to the last kill (same steering/odometer proof as every
        // other leg). Arrival radius is generous: the drop spawns at the
        // corpse and pickup interest is wide.
        if (SteerToward(Player, LastKillLocation, 150.0f, Now, TEXT("corpse")))
        {
            Step = EBotStep::LootPickup;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at corpse, resolving pickup"));
            BotShot(TEXT("corpse"));
            return;
        }
        if (Now - LootLegStartTime > LootTimeoutSec)
        {
            Finish(false, TEXT("corpse-timeout"));
        }
        return;
    }

    if (Step == EBotStep::LootPickup)
    {
        if (bStubLoot)
        {
            // Offline: no server, so the drop itself is stubbed — through the
            // real SpawnDroppedItem path — and resolved through the real
            // inventory/pickup calls. Walking here and the asserts are real.
            if (!LootDropActor.IsValid() && !bPickupRequested)
            {
                if (!SpawnStubDrop(Player))
                {
                    Finish(false, TEXT("stub-drop-failed"));
                    return;
                }
            }
            if (bStubHarvest)
            {
                // Offline: no harvest progress server exists. The completion
                // is stubbed through the real SetHarvested path (same flag
                // the server-driven harvest sets) before the stub pickup.
                if (ABasicMOB* Corpse = ResolveCorpseActor())
                {
                    if (!Corpse->HasBeenHarvested())
                    {
                        Corpse->SetHarvested(true);
                        UE_LOG(LogTemp, Warning, TEXT("%s_HARVEST_STUB uid=%d marked harvested (offline progress)"),
                            *TombstonePrefix, CorpseUID);
                    }
                }
            }
            ResolveStubLoot(Player);
            const int32 Have = CountInventoryItems();
            const int32 HaveQty = SumInventoryQuantities();
            BotAssert(Have == InitialInvCount + 1, TEXT("loot-inv-plus-one"), Have, InitialInvCount + 1);
            BotAssert(HaveQty == InitialInvQty + 1, TEXT("loot-qty-plus-one"), HaveQty, InitialInvQty + 1);
            BotShot(TEXT("loot"));
            if (bQuestPhase)
            {
                QuestLootDone(Now);
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
            return;
        }
        // Live: the real game flow — harvest the victim's corpse, inspect
        // its loot, pick it all up. (World itemDrop packets on the dev
        // стенд carry empty item lists; corpse loot is the loot path.)
        UHarvestManager* Harvest = GameInstance->GetHarvestManager();
        if (!Harvest)
        {
            Finish(false, TEXT("no-harvest-manager"));
            return;
        }
        ABasicMOB* Corpse = ResolveCorpseActor();
        // Post-pickup the server may destroy the corpse actor — from here
        // on only the inventory snapshot matters, not the corpse.
        if (bPickupRequested || IsValid(Corpse))
        {
            if (CountInventoryItems() > InitialInvCount || SumInventoryQuantities() > InitialInvQty)
            {
                BotAssert(true, TEXT("loot-inv-grew"), 1, 1);
                BotShot(TEXT("loot"));
                if (bQuestPhase)
                {
                    QuestLootDone(Now);
                    return;
                }
                Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
                return;
            }
        }
        if (!IsValid(Corpse))
        {
            // Corpse consumed by the pickup (server removes looted corpses):
            // keep polling the inventory until the pickup budget expires.
            // Corpse lost BEFORE any pickup request is a real failure.
            if (!bPickupRequested)
            {
                Finish(false, TEXT("corpse-gone"));
                return;
            }
            if (Now - LootLegStartTime > LootTimeoutSec)
            {
                BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
                Finish(false, TEXT("pickup-timeout"));
            }
            return;
        }
        if (CorpseUID <= 0)
        {
            CorpseUID = FCString::Atoi(*Corpse->GetMOBUId());
        }
        // Phase 1 (harvest_run only): harvest the corpse through the real
        // progress round-trip (server duration). loot_pickup skips straight
        // to inspect. Already-harvested corpses skip to inspect in both.
        if (bHarvestPhase && Corpse->CanBeHarvested())
        {
            if (!bHarvestRequested)
            {
                Player->SetLockedTarget(Corpse);
                Harvest->TryHarvestSpecificCorpse(Corpse);
                bHarvestRequested = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT harvest requested on corpse uid=%d"), CorpseUID);
            }
            if (Now - LootLegStartTime > HarvestTimeoutSec)
            {
                Finish(false, TEXT("harvest-timeout"));
            }
            return;
        }
        // Phase 2: inspect the harvested corpse's loot.
        if (!bInspectRequested)
        {
            Harvest->InspectCorpseLoot(CorpseUID);
            bInspectRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT loot inspect requested uid=%d"), CorpseUID);
            return;
        }
        // Server-confirmed empty corpse is a CONTENT signal (no loot table),
        // reported loudly rather than timing out — except in quest mode,
        // where an empty corpse just means this victim had no quest item:
        // move on through the same checkpoint (hunt next or turn in).
        if (Harvest->IsCorpseEmpty(CorpseUID))
        {
            if (bQuestPhase)
            {
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT corpse uid=%d empty, moving on (%d/%d)"),
                    CorpseUID, Kills, TargetKills);
                QuestLootDone(Now);
                return;
            }
            BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
            Finish(false, TEXT("empty-corpse"));
            return;
        }
        if (Harvest->GetCurrentAvailableLoot().Num() == 0)
        {
            if (Now - LootLegStartTime > LootTimeoutSec)
            {
                BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
                Finish(false, TEXT("inspect-timeout"));
            }
            return;
        }
        // Phase 3: pick it all up, then fall through to the shared
        // inventory-growth poll below.
        if (!bPickupRequested)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT pickup-all requested (%d item(s))"),
                Harvest->GetCurrentAvailableLoot().Num());
            Harvest->PickupAllLoot();
            bPickupRequested = true;
            return;
        }
        // Server confirmation lands as an inventory snapshot (stacks may
        // merge, so count OR quantity growth counts as proof).
        if (CountInventoryItems() > InitialInvCount || SumInventoryQuantities() > InitialInvQty)
        {
            BotAssert(true, TEXT("loot-inv-grew"), 1, 1);
            BotShot(TEXT("loot"));
            if (bQuestPhase)
            {
                QuestLootDone(Now);
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - LootLegStartTime > LootTimeoutSec)
        {
            BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
            Finish(false, TEXT("pickup-timeout"));
        }
        return;
    }

    if (Step == EBotStep::ExpectDeath)
    {
        if (bStubDeath)
        {
            // Offline: no server damage exists, so the killing blow is an
            // honest stub through the real SetDead(true) path (same call the
            // server-driven death takes). Death screen + asserts are real.
            ApplyStubDeath(Player);
            DeathLegStartTime = Now;
            Step = EBotStep::AssertDead;
            StepStartTime = Now;
            return;
        }
        // Live: find a killer and stand down in range until real server
        // damage lands. Never attacks (bPassiveCombat handoff or direct).
        // Killers that deal zero damage for KillerDummySec are dummies
        // (wrong faction, leashed, geometry-separated) — exclude and move on.
        if (!bDeathWatchInit)
        {
            bDeathWatchInit = true;
            DeathWatchHP = Player->GetPlayerCurrentHPPoints();
            LastDamageTime = Now;
        }
        if (!TargetMob.IsValid() || TargetMob->GetMOBIsDead())
        {
            TargetMob = FindNearestAliveMobExcluding(ExcludedKillerUIDs);
            if (!TargetMob.IsValid())
            {
                Finish(false, TEXT("no-killer"));
                return;
            }
            // Fresh killer, fresh pull + fresh chase baseline.
            bProvoked = false;
            ChaseCheckTime = Now;
            ChaseCheckDist = Dist2D(PlayerAt, TargetMob->GetActorLocation());
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT offering target to '%s' (passive, dist %.0f)"),
                *TargetMob->GetMobName(), ChaseCheckDist);
        }
        ABasicMOB* Killer = TargetMob.Get();
        const float PassiveRange = Player->GetCurrentSkillRange();
        const float KillerDist = Dist2D(PlayerAt, Killer->GetActorLocation());
        if (KillerDist > PassiveRange * ArrivalFraction)
        {
            SteerToward(Player, Killer->GetActorLocation(), PassiveRange * ArrivalFraction, Now, *Killer->GetMobName());
            // No distance gain in ChaseStallSec: geometry the server won't
            // let us cross (moves get positionCorrection-reverted). Exclude
            // and sample the next killer instead of pushing a wall.
            if (Now - ChaseCheckTime > ChaseStallSec)
            {
                if (ChaseCheckDist - KillerDist < ChaseMinGain)
                {
                    const int32 StuckUID = FCString::Atoi(*Killer->GetMOBUId());
                    ExcludedKillerUIDs.Add(StuckUID);
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT killer '%s' unreachable (gain %.0f in %.0fs) — excluding uid=%d, switching"),
                        *Killer->GetMobName(), ChaseCheckDist - KillerDist, ChaseStallSec, StuckUID);
                    TargetMob.Reset();
                    return;
                }
                ChaseCheckTime = Now;
                ChaseCheckDist = KillerDist;
            }
            if (Now - DeathLegStartTime > DeathTimeoutSec)
            {
                Finish(false, TEXT("killer-unreachable"));
            }
            return;
        }
        if (Player->GetLockedTarget() != Killer)
        {
            Player->SetLockedTarget(Killer);
        }
        if (!bProvoked)
        {
            // Single honest pull: real server hit, starts the mob's
            // retaliation. The loop stops below — the bot must die, not win.
            Player->OnAttackInput();
            bProvoked = true;
            ProvokeTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT provoked '%s' (one hit, standing down next)"),
                *Killer->GetMobName());
        }
        else if (Player->bIsAutoAttacking && Now - ProvokeTime > 3.0)
        {
            Player->StopAutoAttack();
        }
        const int32 NowHP = Player->GetPlayerCurrentHPPoints();
        if (NowHP < DeathWatchHP)
        {
            // Taking real server damage — keep standing here.
            DeathWatchHP = NowHP;
            LastDamageTime = Now;
        }
        else if (Now - LastDamageTime > KillerDummySec)
        {
            const int32 DummyUID = FCString::Atoi(*Killer->GetMOBUId());
            ExcludedKillerUIDs.Add(DummyUID);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT killer '%s' dealt no damage in %.0fs — excluding uid=%d, switching"),
                *Killer->GetMobName(), KillerDummySec, DummyUID);
            TargetMob.Reset();
            DeathWatchHP = NowHP;
            LastDamageTime = Now;
            return;
        }
        if (Now - DeathLegStartTime > DeathTimeoutSec)
        {
            Finish(false, FString::Printf(TEXT("no-death hp=%d dist=%.0f"), NowHP, KillerDist));
        }
        // Still alive: keep standing here; the top-of-tick dead check
        // redirects to AssertDead the moment a server hit lands.
        return;
    }

    if (Step == EBotStep::AssertDead)
    {
        // Entered only while dead (top-of-tick redirect or stub handoff).
        if (!Player->GetIsDead())
        {
            Finish(false, TEXT("death-not-registered"));
            return;
        }
        if (!bDeathAsserted)
        {
            bDeathAsserted = true;
            BotAssert(true, TEXT("player-dead"), 1, 1);
            const bool bScreen = IsDeathScreenVisible();
            BotAssert(bScreen, TEXT("death-screen"), bScreen ? 1 : 0, 1);
            BotShot(TEXT("death"));
            if (bStubDeath)
            {
                ApplyStubRevive(Player);
            }
            else if (!bRespawnRequested)
            {
                Player->OnRespawnClicked();
                bRespawnRequested = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT respawn requested"));
            }
            RespawnLegStartTime = FPlatformTime::Seconds();
            Step = EBotStep::RespawnWait;
            StepStartTime = RespawnLegStartTime;
        }
        return;
    }

    if (Step == EBotStep::RespawnWait)
    {
        // Offline revive is synchronous (stub); live waits for the server
        // respawnResult packet that clears the dead flag.
        if (Player->GetIsDead())
        {
            if (!bStubDeath && !bRespawnRequested)
            {
                Player->OnRespawnClicked();
                bRespawnRequested = true;
            }
            if (FPlatformTime::Seconds() - RespawnLegStartTime > RespawnTimeoutSec)
            {
                BotAssert(false, TEXT("player-revived"), 0, 1);
                Finish(false, TEXT("respawn-timeout"));
            }
            return;
        }
        const int32 HP = Player->GetPlayerCurrentHPPoints();
        if (HP <= 0)
        {
            // Live: the dead flag clears in respawnResult, the HP snapshot
            // (stats_update) arrives a beat later. Wait for it briefly
            // instead of asserting on the stale zero.
            const double NowHP = FPlatformTime::Seconds();
            if (ReviveSeenTime <= 0.0)
            {
                ReviveSeenTime = NowHP;
                BotAssert(true, TEXT("player-revived"), 1, 1);
            }
            if (NowHP - ReviveSeenTime > 10.0)
            {
                BotAssert(false, TEXT("respawn-hp"), HP, 1);
                Finish(false, TEXT("respawn-hp-timeout"));
            }
            return;
        }
        if (ReviveSeenTime <= 0.0)
        {
            ReviveSeenTime = FPlatformTime::Seconds();
            BotAssert(true, TEXT("player-revived"), 1, 1);
        }
        BotAssert(HP > 0, TEXT("respawn-hp"), HP, 1);
        const bool bScreenHidden = !IsDeathScreenVisible();
        BotAssert(bScreenHidden, TEXT("death-screen-hidden"), bScreenHidden ? 1 : 0, 1);
        if (bStubDeath)
        {
            // Offline revive point is deterministic (run spawn).
            const float Home = Dist2D(Player->GetActorLocation(), RespawnPoint);
            BotAssert(Home < 300.0f, TEXT("respawn-point"), (int32)Home, 0);
        }
        BotShot(TEXT("respawned"));
        Finish(Failures == 0, Failures == 0 ? TEXT("death-respawn-done") : TEXT("assert-failures"));
        return;
    }

    if (Step == EBotStep::RecoverRevive)
    {
        // Stale server-side death (previous run never revived): wait for
        // the revival, then start the run over. Never asserts — recovery
        // is hygiene, the scenario proof comes afterwards.
        if (!Player->GetIsDead())
        {
            RunStartPos = Player->GetActorLocation();
            // Re-aim the locomotion waypoint from the revive point (the one
            // computed at Start belongs to the death position).
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
            LastChaseProgressTime = Now;
            Step = EBotStep::GotoWaypoint;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT recovered from stale death, starting run"));
            return;
        }
        if (FPlatformTime::Seconds() - RespawnLegStartTime > RespawnTimeoutSec)
        {
            Finish(false, TEXT("spawned-dead"));
        }
        return;
    }

    if (Step == EBotStep::VendorWalk)
    {
        // Honest walk to the vendor (server validates range from its
        // stored position — teleports would fail the shop open).
        if (SteerToward(Player, VendorPos, 150.0f, Now, TEXT("vendor")))
        {
            Step = EBotStep::VendorSell;
            ShopLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at vendor npc=%d, opening shop"), VendorNpcId);
            BotShot(TEXT("vendor"));
            return;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            Finish(false, TEXT("vendor-unreachable"));
        }
        return;
    }

    if (Step == EBotStep::VendorSell)
    {
        UVendorManager* Vend = GameInstance->GetVendorManager();
        UInventoryManager* Inv = GameInstance->GetInventoryManager();
        if (!Vend || !Inv)
        {
            Finish(false, TEXT("no-vendor"));
            return;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        if (!bVendorShopOpened)
        {
            if (Vend->GetCurrentShop().npcId == VendorNpcId)
            {
                bVendorShopOpened = true;
                BotAssert(true, TEXT("vendor-shop-open"), VendorNpcId, VendorNpcId);
                GoldBeforeSell = GetGold();
                SellInstanceId = FindSellableInstanceId(SellStartQty);
                if (SellInstanceId <= 0)
                {
                    Finish(false, TEXT("no-junk"));
                    return;
                }
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT shop open (npc=%d) gold=%d, selling instance=%d qty=%d"),
                    VendorNpcId, GoldBeforeSell, SellInstanceId, SellStartQty);
            }
            else
            {
                if (!bShopRequestSent)
                {
                    Vend->RequestOpenVendorShop(CharId, VendorNpcId, Player->GetActorLocation());
                    bShopRequestSent = true;
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT vendor shop requested npc=%d"), VendorNpcId);
                }
                if (Now - ShopLegStartTime > ShopTimeoutSec)
                {
                    Finish(false, TEXT("shop-timeout"));
                }
            }
            return;
        }
        if (!bSellRequested)
        {
            Vend->RequestSellItem(CharId, VendorNpcId, SellInstanceId, 1, Player->GetActorLocation());
            bSellRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT sell requested instance=%d"), SellInstanceId);
            return;
        }
        // Server pushes the post-sale inventory snapshot (same packet the
        // shop UI reads) — gold up AND stack down by exactly one.
        if (GetGold() > GoldBeforeSell && GetQtyByInstance(SellInstanceId) == SellStartQty - 1)
        {
            BotAssert(true, TEXT("sell-gold-up"), 1, 1);
            BotAssert(true, TEXT("sell-qty-down"), 1, 1);
            BotShot(TEXT("sold"));
            Step = EBotStep::VendorRepair;
            ShopLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT sold: gold %d -> %d"), GoldBeforeSell, GetGold());
            return;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("sell-gold-up"), GetGold(), GoldBeforeSell + 1);
            Finish(false, TEXT("sell-timeout"));
        }
        return;
    }

    if (Step == EBotStep::VendorRepair)
    {
        URepairManager* Rep = GameInstance->GetRepairManager();
        if (!Rep)
        {
            Finish(false, TEXT("no-repair"));
            return;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        if (!bRepairShopOpened)
        {
            if (Rep->GetCurrentShop().npcId == VendorNpcId)
            {
                bRepairShopOpened = true;
                BotAssert(true, TEXT("repair-shop-open"), VendorNpcId, VendorNpcId);
                GoldBeforeRepair = GetGold();
                RepairInstanceId = 0;
                for (const FRepairShopItemData& Item : Rep->GetCurrentShop().items)
                {
                    if (Item.durabilityMax > 0 && Item.durabilityCurrent < Item.durabilityMax && Item.repairCost > 0)
                    {
                        RepairInstanceId = Item.inventoryItemId;
                        RepairMaxDur = Item.durabilityMax;
                        break;
                    }
                }
                if (RepairInstanceId <= 0)
                {
                    Finish(false, TEXT("no-damaged"));
                    return;
                }
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair shop open, target instance=%d maxdur=%d gold=%d"),
                    RepairInstanceId, RepairMaxDur, GoldBeforeRepair);
            }
            else
            {
                if (!bRepairShopRequestSent)
                {
                    Rep->RequestOpenRepairShop(CharId, VendorNpcId, Player->GetActorLocation());
                    bRepairShopRequestSent = true;
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair shop requested npc=%d"), VendorNpcId);
                }
                if (Now - ShopLegStartTime > ShopTimeoutSec)
                {
                    Finish(false, TEXT("repair-shop-timeout"));
                }
            }
            return;
        }
        if (!bRepairRequested)
        {
            Rep->RequestRepairItem(CharId, VendorNpcId, RepairInstanceId, Player->GetActorLocation());
            bRepairRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair requested instance=%d"), RepairInstanceId);
            return;
        }
        // The repaired row drops OUT of the shop list (only damaged rows
        // are listed), so durability is read from the server inventory
        // snapshot + gold must have dropped by the repair cost.
        const int32 NowDur = GetDurabilityByInstance(RepairInstanceId);
        if (NowDur == RepairMaxDur && GetGold() < GoldBeforeRepair)
        {
            BotAssert(true, TEXT("repair-durability-max"), NowDur, RepairMaxDur);
            BotAssert(GetGold() < GoldBeforeRepair, TEXT("repair-gold-down"), GetGold(), GoldBeforeRepair);
            BotShot(TEXT("repaired"));
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repaired: durability %d, gold %d -> %d"),
                NowDur, GoldBeforeRepair, GetGold());
            Finish(Failures == 0, Failures == 0 ? TEXT("vendor-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("repair-durability-max"), NowDur, RepairMaxDur);
            Finish(false, TEXT("repair-timeout"));
        }
        return;
    }

    if (Step == EBotStep::ChatExchange)
    {
        UChatManager* Chat = GameInstance->GetChatManager();
        if (!Chat)
        {
            Finish(false, TEXT("no-chat"));
            return;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        if (!bChatSent)
        {
            // Local chat only travels 50u server-side. Both sides anchor to
            // the SAME static rendezvous point (arrive tight) instead of
            // chasing the peer actor — replicated peer positions lag too
            // much to converge on. Then settle (server positions lag the
            // client tick) and speak.
            if (Dist2D(PlayerAt, RendezvousPoint) > 20.0f)
            {
                PeerCloseSince = 0.0;
                SteerToward(Player, RendezvousPoint, 20.0f, Now, TEXT("chat-anchor"));
                if (Now - ChatLegStartTime > ChatTimeoutSec)
                {
                    Finish(false, TEXT("peer-unreachable"));
                }
                return;
            }
            if (!IsValid(FindPeerActor()))
            {
                Finish(false, TEXT("peer-lost"));
                return;
            }
            if (PeerCloseSince <= 0.0)
            {
                PeerCloseSince = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT anchored at rendezvous, settling before speaking"));
                return;
            }
            if (Now - PeerCloseSince < 3.0)
            {
                return;
            }
            // One short "local" message each; both sides send, both assert
            // the peer's arrival (own echo, if any, is filtered by sender).
            Chat->SendChatMessage(TEXT("local"), FString::Printf(TEXT("hi-%d"), OwnCharId));
            bChatSent = true;
            ChatLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT chat sent hi-%d from (%.0f,%.0f,%.0f)"),
                OwnCharId, PlayerAt.X, PlayerAt.Y, PlayerAt.Z);
            return;
        }
        // Peer message = hi-* from anyone but us.
        PeerChatText.Empty();
        for (const FChatMessageStruct& Msg : Chat->GetMessageLog())
        {
            if (!Msg.bIsError && Msg.senderId > 0 && Msg.senderId != OwnCharId && Msg.text.StartsWith(TEXT("hi-")))
            {
                PeerChatText = Msg.text;
                break;
            }
        }
        if (!PeerChatText.IsEmpty())
        {
            BotAssert(true, TEXT("peer-msg-received"), 1, 1);
            // The bubble renders on the peer actor's nameplate through the
            // same ShowChatBubble call live players trigger.
            bool bBubbleOk = false;
            if (ABasicPlayer* Peer = FindPeerActor())
            {
                if (UPlayerNameplateComponent* Plate = Peer->GetNameplateComponent())
                {
                    bBubbleOk = Plate->GetLastBubbleText() == PeerChatText;
                }
            }
            BotAssert(bBubbleOk, TEXT("peer-bubble"), bBubbleOk ? 1 : 0, 1);
            BotShot(TEXT("chat"));
            Finish(Failures == 0, Failures == 0 ? TEXT("chat-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - ChatLegStartTime > ChatTimeoutSec)
        {
            BotAssert(false, TEXT("peer-msg-received"), 0, 1);
            Finish(false, TEXT("chat-timeout"));
        }
        return;
    }

    if (Step == EBotStep::QuestWalk)
    {
        // Honest walk to the quest giver (dialogue range is server-side).
        if (SteerToward(Player, QuestNpcPos, 150.0f, Now, TEXT("quest-npc")))
        {
            Step = EBotStep::QuestAccept;
            QuestLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at quest npc=%d, opening dialogue"), QuestNpcId);
            return;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            Finish(false, TEXT("quest-unreachable"));
        }
        return;
    }

    if (Step == EBotStep::QuestAccept)
    {
        UDialogueManager* Dlg = GameInstance->GetDialogueManager();
        UQuestManager* Qst = GameInstance->GetQuestManager();
        if (!Dlg || !Qst)
        {
            Finish(false, TEXT("no-dialogue"));
            return;
        }
        // Accepted already (choice landed, or a rerun mid-chain): hunt.
        if (Qst->HasQuest(QuestId))
        {
            BotAssert(true, TEXT("quest-accepted"), QuestId, QuestId);
            BotShot(TEXT("accepted"));
            if (Dlg->IsDialogueActive())
            {
                Dlg->CloseDialogue();
            }
            // Objectives run on the shared combat/loot machinery (2 real
            // kills + harvest + loot); the server credits them to the quest.
            TargetMob.Reset();
            Step = EBotStep::GotoTarget;
            StepStartTime = Now;
            LastChasePos = PlayerAt;
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT quest accepted, hunting objectives"));
            return;
        }
        if (!EnsureDialogueOpen(QuestNpcId, Now))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("accept-timeout"));
            }
            return;
        }
        if (const int32 AcceptEdge = FindGoalEdge(false))
        {
            if (!bChoiceSent)
            {
                Dlg->SendChoice(AcceptEdge);
                bChoiceSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT accept choice sent edge=%d"), AcceptEdge);
            }
            // Quest grant lands as a journal push; polled at the top on the
            // next ticks (HasQuest branch above).
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("accept-timeout"));
            }
            return;
        }
        // No offer on this node: walk the dialogue (greeting -> hub).
        if (DialogueClicks >= 10)
        {
            Finish(false, TEXT("dialogue-loop"));
            return;
        }
        const int32 NodeId = Dlg->GetCurrentNode().nodeId;
        if (const int32 AdvanceEdge = FindAdvanceEdge())
        {
            if (NodeId != LastAdvanceNodeId)
            {
                Dlg->SendChoice(AdvanceEdge);
                LastAdvanceNodeId = NodeId;
                ++DialogueClicks;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT dialogue advance edge=%d (node %d)"),
                    AdvanceEdge, NodeId);
            }
            return;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT no accept/advance edge in node %d (%d choices)"),
                NodeId, Dlg->GetCurrentNode().choices.Num());
            Finish(false, TEXT("no-accept-choice"));
        }
        return;
    }

    if (Step == EBotStep::QuestTurnin)
    {
        UDialogueManager* Dlg = GameInstance->GetDialogueManager();
        UQuestManager* Qst = GameInstance->GetQuestManager();
        if (!Dlg || !Qst)
        {
            Finish(false, TEXT("no-dialogue"));
            return;
        }
        // Walk back first (objectives may have pulled us far away). The
        // server gates dialogue on proximity: only talk once arrived.
        if (!SteerToward(Player, QuestNpcPos, 150.0f, Now, TEXT("turnin-npc")))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("turnin-unreachable"));
            }
            return;
        }
        if (!EnsureDialogueOpen(QuestNpcId, Now))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("turnin-timeout"));
            }
            return;
        }
        if (const int32 TurninEdge = FindGoalEdge(true))
        {
            if (!bChoiceSent)
            {
                Dlg->SendChoice(TurninEdge);
                bChoiceSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turnin choice sent edge=%d"), TurninEdge);
                return;
            }
        }
        // Reward ground truth (checked regardless of the current node: the
        // turn-in choice advances past the turn-in edge): +5g floor (loot
        // gold may add more) and the freshly granted potion.
        if (bChoiceSent && GetGold() >= GoldBeforeReward + 5 && HasItemTemplate(46))
        {
            const FQuestProgressData Q = Qst->GetQuestData(QuestId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turned in: gold %d -> %d queststate='%s'"),
                GoldBeforeReward, GetGold(), *Q.state);
            BotAssert(true, TEXT("quest-reward-gold"), 1, 1);
            BotAssert(true, TEXT("quest-reward-item"), 46, 46);
            BotShot(TEXT("turnedin"));
            Dlg->CloseDialogue();
            Finish(Failures == 0, Failures == 0 ? TEXT("quest-done") : TEXT("assert-failures"));
            return;
        }
        if (bChoiceSent)
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                BotAssert(false, TEXT("quest-reward-gold"), GetGold(), GoldBeforeReward + 5);
                Finish(false, TEXT("turnin-timeout"));
            }
            return;
        }
        // Objectives not yet credited server-side, or a hub without the
        // turn-in: advance neutral hubs, budget the silence.
        if (DialogueClicks >= 10)
        {
            Finish(false, TEXT("dialogue-loop"));
            return;
        }
        const int32 NodeId = Dlg->GetCurrentNode().nodeId;
        if (const int32 AdvanceEdge = FindAdvanceEdge())
        {
            if (NodeId != LastAdvanceNodeId)
            {
                Dlg->SendChoice(AdvanceEdge);
                LastAdvanceNodeId = NodeId;
                ++DialogueClicks;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turnin dialogue advance edge=%d (node %d)"),
                    AdvanceEdge, NodeId);
            }
            return;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            const FQuestProgressData Q = Qst->GetQuestData(QuestId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT no turnin edge in node %d (%d choices) queststate='%s' step='%s' %d/%d"),
                NodeId, Dlg->GetCurrentNode().choices.Num(), *Q.state, *Q.currentStepKey, Q.stepCurrentCount, Q.stepRequiredCount);
            Finish(false, TEXT("no-turnin-choice"));
        }
        return;
    }

    if (Step == EBotStep::SkillWalk)
    {
        // Honest walk to the trainer (shop range is server-side).
        if (SteerToward(Player, SkillNpcPos, 150.0f, Now, TEXT("trainer")))
        {
            Step = EBotStep::SkillLearn;
            CharLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at trainer npc=%d, opening skill shop"), SkillNpcId);
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            Finish(false, TEXT("skills-unreachable"));
        }
        return;
    }

    if (Step == EBotStep::SkillLearn)
    {
        USkillShopManager* Shop = GameInstance->GetSkillShopManager();
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!Shop || !PSM)
        {
            Finish(false, TEXT("no-skills"));
            return;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        if (Shop->GetCurrentShop().npcId != SkillNpcId)
        {
            if (!bShopReqSent)
            {
                Shop->RequestOpenSkillShop(SkillNpcId, Player->GetActorLocation());
                bShopReqSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT skill shop requested npc=%d"), SkillNpcId);
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                Finish(false, TEXT("skill-shop-timeout"));
            }
            return;
        }
        BotAssert(true, TEXT("skill-shop-open"), SkillNpcId, SkillNpcId);
        // Learn power_slash then battle_cry (one in flight at a time).
        FString Next;
        if (!PSM->HasSkill(TEXT("power_slash"))) { Next = TEXT("power_slash"); }
        else if (!PSM->HasSkill(TEXT("battle_cry"))) { Next = TEXT("battle_cry"); }
        if (!Next.IsEmpty())
        {
            if (!bLearnRequested || LastLearnSlug != Next)
            {
                Shop->RequestLearnSkill(SkillNpcId, Next);
                bLearnRequested = true;
                LastLearnSlug = Next;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT learn requested %s"), *Next);
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                BotAssert(false, TEXT("skill-learned"), 0, 1);
                Finish(false, TEXT("learn-timeout"));
            }
            return;
        }
        BotAssert(true, TEXT("skill-learned-power"), 1, 1);
        BotAssert(true, TEXT("skill-learned-cry"), 1, 1);
        BotShot(TEXT("learned"));
        Step = EBotStep::SkillCast;
        CharLegStartTime = Now;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT skills learned, hunting cast target"));
        return;
    }

    if (Step == EBotStep::SkillCast)
    {
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!PSM)
        {
            Finish(false, TEXT("no-skills"));
            return;
        }
        if (!TargetMob.IsValid() || TargetMob->GetMOBIsDead())
        {
            if (bCastSent && TargetMob.IsValid())
            {
                // Our hit landed lethally: the deferred server death flips
                // the flag instead of an HP number. Damage proven — keep the
                // corpse pointer (cooldown/mana latches still poll below),
                // do NOT repick and lose the kill.
                bDamageSeen = true;
            }
            else
            {
                TargetMob = FindNearestAliveMobExcluding(ExcludedKillerUIDs);
                if (!TargetMob.IsValid())
                {
                    Finish(false, TEXT("no-cast-target"));
                    return;
                }
                ChaseTargetUID = 0;
            }
        }
        ABasicMOB* Target = TargetMob.Get();
        const int32 TargetUid = FCString::Atoi(*Target->GetMOBUId());
        const float CastRange = Player->GetCurrentSkillRange();
        // Post-cast the mob may flee: poll the hit regardless of range.
        // The range gate applies only to the first cast. Server
        // confirmation lands piecemeal: cooldown arms on initiation, mana
        // on stats_update, damage on the deferred result flush (which may
        // lag ~12s and may only flip the dead flag — accept HP drop OR
        // death as the damage proof).
        if (bCastSent)
        {
            const int32 MobHpNow = Target->GetMOBCurrentHealth();
            const int32 ManaNow = Player->GetPlayerCurrentMPPoints();
            bDamageSeen = bDamageSeen || (MobHpNow < MobHpBefore) || Target->GetMOBIsDead();
            bCooldownSeen = bCooldownSeen || PSM->IsSkillOnCooldown(TEXT("power_slash"));
            bManaSeen = bManaSeen || (ManaNow < ManaBefore);
            if (bDamageSeen && bCooldownSeen && bManaSeen)
            {
                BotAssert(true, TEXT("skill-damage"), 1, 1);
                BotAssert(true, TEXT("skill-cooldown"), 1, 1);
                BotAssert(true, TEXT("skill-mana"), 1, 1);
                BotShot(TEXT("cast"));
                Step = EBotStep::SkillBuff;
                CharLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT power_slash confirmed: mobhp %d -> %d mana %d -> %d"),
                    MobHpBefore, MobHpNow, ManaBefore, ManaNow);
                return;
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                BotAssert(bDamageSeen, TEXT("skill-damage"), bDamageSeen ? 1 : 0, 1);
                BotAssert(bCooldownSeen, TEXT("skill-cooldown"), bCooldownSeen ? 1 : 0, 1);
                BotAssert(bManaSeen, TEXT("skill-mana"), bManaSeen ? 1 : 0, 1);
                Finish(false, TEXT("cast-timeout"));
            }
            return;
        }
        if (Dist2D(PlayerAt, Target->GetActorLocation()) > CastRange * ArrivalFraction)
        {
            SteerToward(Player, Target->GetActorLocation(), CastRange * ArrivalFraction, Now, *Target->GetMobName());
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                Finish(false, TEXT("cast-unreachable"));
            }
            return;
        }
        if (Player->GetLockedTarget() != Target)
        {
            Player->SetLockedTarget(Target);
        }
        MobHpBefore = Target->GetMOBCurrentHealth();
        ManaBefore = Player->GetPlayerCurrentMPPoints();
        PSM->TryCastSkill(TEXT("power_slash"), TargetUid, ECasterType::Mob);
        bCastSent = true;
        bDamageSeen = false;
        bCooldownSeen = false;
        bManaSeen = false;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT power_slash cast on uid=%d (mobhp=%d mana=%d)"),
            TargetUid, MobHpBefore, ManaBefore);
        return;
    }

    if (Step == EBotStep::SkillBuff)
    {
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!PSM)
        {
            Finish(false, TEXT("no-skills"));
            return;
        }
        if (!bBuffSent)
        {
            PSM->TryCastSkill(TEXT("battle_cry"), 0, ECasterType::Player);
            bBuffSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT battle_cry cast (self)"));
            return;
        }
        // The buff lands as a server stats_update activeEffects entry —
        // the same snapshot ActiveEffectsWidget renders.
        if (HasActiveEffect(TEXT("battle_cry_phys_atk")))
        {
            BotAssert(true, TEXT("buff-active"), 1, 1);
            BotShot(TEXT("buff"));
            Finish(Failures == 0, Failures == 0 ? TEXT("skills-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("buff-active"), 0, 1);
            Finish(false, TEXT("buff-timeout"));
        }
        return;
    }

    if (Step == EBotStep::EquipOn)
    {
        UEquipmentManager* EqMgr = GameInstance->GetEquipmentManager();
        if (!EqMgr)
        {
            Finish(false, TEXT("no-equipment"));
            return;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        const FEquipmentSlotData Slot = EqMgr->GetSlot(EquipSlotSlug);
        if (Slot.bIsOccupied && EquipInstanceId <= 0)
        {
            // Idempotent entry: already equipped (e.g. a previous run left
            // it on) — verify slot + visual, then continue the round-trip.
            bWasPreEquipped = true;
            EquipInstanceId = Slot.inventoryItemId;
            EquipItemSlug = Slot.itemSlug;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT slot %s pre-equipped instance=%d, verifying"),
                *EquipSlotSlug, EquipInstanceId);
        }
        if (EquipInstanceId <= 0)
        {
            EquipInstanceId = FindEquippableMainHand(EquipItemSlug);
            if (EquipInstanceId <= 0)
            {
                Finish(false, TEXT("no-equippable"));
                return;
            }
            EqMgr->RequestEquipItem(CharId, EquipInstanceId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT equip requested instance=%d '%s'"),
                EquipInstanceId, *EquipItemSlug);
            return;
        }
        if (Slot.bIsOccupied && Slot.inventoryItemId == EquipInstanceId)
        {
            BotAssert(true, TEXT("equip-slot"), 1, 1);
            // Visual proof: the mesh the game attached for this slot.
            bool bVisualOk = false;
            if (UEquipmentVisualComponent* Vis = Player->GetEquipmentVisualComponent())
            {
                bVisualOk = Vis->GetItemSlugForSlot(EquipSlotSlug) == EquipItemSlug
                    && Vis->GetSlotComponent(EquipSlotSlug) != nullptr;
            }
            BotAssert(bVisualOk, TEXT("equip-visual"), bVisualOk ? 1 : 0, 1);
            BotShot(TEXT("equipped"));
            if (bRestorePass)
            {
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT pre-equipped item restored"));
                Finish(Failures == 0, Failures == 0 ? TEXT("equip-done") : TEXT("assert-failures"));
                return;
            }
            Step = EBotStep::EquipOff;
            CharLegStartTime = Now;
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("equip-slot"), 0, EquipInstanceId);
            Finish(false, TEXT("equip-timeout"));
        }
        return;
    }

    if (Step == EBotStep::EquipOff)
    {
        UEquipmentManager* EqMgr = GameInstance->GetEquipmentManager();
        if (!EqMgr)
        {
            Finish(false, TEXT("no-equipment"));
            return;
        }
        if (!bUnequipSent)
        {
            EqMgr->RequestUnequipItem(GameInstance->GetCurrentCharacterID(), EquipSlotSlug);
            bUnequipSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT unequip requested slot=%s"), *EquipSlotSlug);
            return;
        }
        const FEquipmentSlotData Slot = EqMgr->GetSlot(EquipSlotSlug);
        if (!Slot.bIsOccupied)
        {
            BotAssert(true, TEXT("unequip-slot"), 1, 1);
            BotShot(TEXT("unequipped"));
            if (bWasPreEquipped)
            {
                // Restore the pre-run state, verify once, then finish.
                EqMgr->RequestEquipItem(GameInstance->GetCurrentCharacterID(), EquipInstanceId);
                bWasPreEquipped = false;
                bUnequipSent = false;
                bRestorePass = true;
                EquipInstanceId = 0;
                Step = EBotStep::EquipOn;
                CharLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT restoring pre-equipped item"));
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("equip-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("unequip-slot"), 1, 0);
            Finish(false, TEXT("unequip-timeout"));
        }
        return;
    }

    if (Step == EBotStep::LevelWait)
    {
        // XP is granted externally mid-run (admin grant_xp): the bot only
        // polls the authoritative progression snapshot.
        const int32 LevelNow = GetProgressionLevel();
        const int32 ExpNow = GetProgressionExp();
        if (LevelNow > StartLevel && ExpNow > StartExp)
        {
            BotAssert(true, TEXT("level-grew"), LevelNow, StartLevel + 1);
            BotAssert(true, TEXT("exp-grew"), 1, 1);
            BotShot(TEXT("levelup"));
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT leveled: %d -> %d exp %d -> %d"),
                StartLevel, LevelNow, StartExp, ExpNow);
            Finish(Failures == 0, Failures == 0 ? TEXT("level-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("level-grew"), LevelNow, StartLevel + 1);
            Finish(false, TEXT("level-timeout"));
        }
        return;
    }

    if (Step == EBotStep::TitleLeg)
    {
        UTitleNetworkHandler* TitleNet = GameInstance->GetTitleNetworkHandler();
        UTitleManager* Titles = GameInstance->GetTitleManager();
        if (!TitleNet || !Titles)
        {
            Finish(false, TEXT("no-titles"));
            return;
        }
        if (!bTitleSent)
        {
            TitleNet->RequestEquipTitle(GameInstance->GetCurrentCharacterID(), TEXT("first_hunter"));
            bTitleSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT equip title first_hunter"));
            return;
        }
        if (Titles->GetEquippedTitleSlug() == TEXT("first_hunter"))
        {
            BotAssert(true, TEXT("title-equipped"), 1, 1);
            BotShot(TEXT("title"));
            Step = EBotStep::EmoteLeg;
            CharLegStartTime = Now;
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("title-equipped"), 0, 1);
            Finish(false, TEXT("title-timeout"));
        }
        return;
    }

    if (Step == EBotStep::EmoteLeg)
    {
        UEmoteNetworkHandler* EmoteNet = GameInstance->GetEmoteNetworkHandler();
        UEmoteManager* Emotes = GameInstance->GetEmoteManager();
        if (!EmoteNet || !Emotes)
        {
            Finish(false, TEXT("no-emotes"));
            return;
        }
        if (!bEmoteSent)
        {
            // Play the first owned emote from the server snapshot — never
            // assume a slug is unlocked (salute is NOT default; wave is).
            const TArray<FEmoteDefinitionData>& Owned = Emotes->GetPlayerEmotesState().emotes;
            if (Owned.Num() == 0)
            {
                Finish(false, TEXT("no-emotes"));
                return;
            }
            EmoteSlug = Owned[0].slug;
            EmoteNet->RequestUseEmote(EmoteSlug);
            bEmoteSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT emote %s sent"), *EmoteSlug);
            return;
        }
        // The server broadcasts back (sender in own range): our actor plays
        // through the same PlayEmoteForCharacter path as any player.
        bool bPlaying = false;
        if (UEmoteComponent* Emote = Player->GetEmoteComponent())
        {
            bPlaying = Emote->IsPlayingEmote() && Emote->GetCurrentEmoteSlug() == EmoteSlug;
        }
        if (bPlaying)
        {
            BotAssert(true, TEXT("emote-playing"), 1, 1);
            BotShot(TEXT("emote"));
            Finish(Failures == 0, Failures == 0 ? TEXT("titles-emotes-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("emote-playing"), 0, 1);
            Finish(false, TEXT("emote-timeout"));
        }
        return;
    }

    ABasicMOB* Mob = TargetMob.Get();
    // Invariant: live target (repick above guarantees it; AssertKill and
    // GotoWaypoint returned). Defensive recover — never dereference stale.
    if (!IsValid(Mob) || Mob->GetMOBIsDead())
    {
        TargetMob.Reset();
        Step = EBotStep::GotoTarget;
        StepStartTime = Now;
        return;
    }
    const FVector MobAt = Mob->GetActorLocation();
    const float Range = Player->GetCurrentSkillRange();
    const float Dist = Dist2D(PlayerAt, MobAt);

    if (Step == EBotStep::GotoTarget)
    {
        // Stall sampling (all modes): a chase with no distance gain is
        // behind geometry the server won't let us cross — exclude the pick
        // and let the repick above sample the next one.
        const int32 ChaseUID = FCString::Atoi(*Mob->GetMOBUId());
        if (ChaseUID != ChaseTargetUID)
        {
            ChaseTargetUID = ChaseUID;
            ChaseCheckTime = Now;
            ChaseCheckDist = Dist;
        }
        else if (Now - ChaseCheckTime > ChaseStallSec)
        {
            if (ChaseCheckDist - Dist < ChaseMinGain)
            {
                ExcludedKillerUIDs.Add(ChaseUID);
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT chase of '%s' stalled (gain %.0f in %.0fs) — excluding uid=%d"),
                    *Mob->GetMobName(), ChaseCheckDist - Dist, ChaseStallSec, ChaseUID);
                TargetMob.Reset();
                ChaseTargetUID = 0;
                return;
            }
            ChaseCheckTime = Now;
            ChaseCheckDist = Dist;
        }
        if (SteerToward(Player, MobAt, Range * ArrivalFraction, Now, *Mob->GetMobName()))
        {
            Player->SetLockedTarget(Mob);
            if (bPassiveCombat)
            {
                // Death leg (live): stand down in range, let the mob hit
                // back. No attack input — the kill must be server-side.
                Step = EBotStep::ExpectDeath;
                DeathLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT in range of '%s', waiting for real damage (passive)"),
                    *Mob->GetMobName());
                return;
            }
            // In range: real lock + real attack loop from here on.
            Player->OnAttackInput();
            bAttackStarted = Player->bIsAutoAttacking;
            CombatStartTime = Now;
            StepStartTime = Now;
            Step = EBotStep::Combat;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT combat vs '%s' (dist %.0f, attack-loop=%d)"),
                *Mob->GetMobName(), Dist, bAttackStarted ? 1 : 0);
            return;
        }

        if (Now - StepStartTime > GotoTimeoutSec)
        {
            Finish(false, TEXT("goto-timeout"));
        }
        return;
    }

    if (Step == EBotStep::Combat)
    {
        // Keep the real loop alive (cooldown/animation/approach code runs;
        // casts cannot confirm offline — no server — by design).
        if (Player->GetLockedTarget() != Mob)
        {
            Player->SetLockedTarget(Mob);
        }
        if (!Player->bIsAutoAttacking)
        {
            Player->OnAttackInput();
            bAttackStarted = bAttackStarted || Player->bIsAutoAttacking;
        }

        // Let the loop run briefly so animation/cooldown code executes, then
        // finish the mob: offline via the honest server-stub hit (same path as
        // devmode.setmobhp), live by waiting for the real server hit.
        if (bStubHits)
        {
            if (Now - CombatStartTime < 2.0f)
            {
                if (Now - StepStartTime > CombatTimeoutSec)
                {
                    Finish(false, TEXT("combat-timeout"));
                }
                return;
            }

            UE_LOG(LogTemp, Warning, TEXT("%s_HIT_STUB '%s' (server-authoritative hit offline)"), *TombstonePrefix, *Mob->GetMobName());
            Mob->SetMOBCurrentHealth(0);
            Mob->Die();
        }
        else
        {
            if (!Mob->GetMOBIsDead() && Now - CombatStartTime > CombatKillTimeoutSec)
            {
                Finish(false, TEXT("no-server-damage"));
                return;
            }
            if (!Mob->GetMOBIsDead())
            {
                // Offline only: the stub hit lands on a fixed beat, so a stall
                // here means broken plumbing. Live waits the full kill budget
                // above (real fights take tens of seconds).
                if (bStubHits && Now - StepStartTime > CombatTimeoutSec)
                {
                    Finish(false, TEXT("combat-timeout"));
                }
                return;
            }
        }
        Step = EBotStep::AssertKill;
        StepStartTime = Now;
        return;
    }
}

void UDevModeBotBrain::BotAssert(bool bPass, const FString& What, int32 Actual, int32 Expected)
{
    if (bPass)
    {
        UE_LOG(LogTemp, Log, TEXT("DEVMODE_ASSERT PASS bot-%s actual=%d"), *What, Actual);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_ASSERT FAIL bot-%s actual=%d expected=%d"), *What, Actual, Expected);
        ++Failures;
    }
}

void UDevModeBotBrain::BotShot(const FString& Tag) const
{
    // Screenshots need a renderer; headless (-nullrhi) runs skip them.
    if (!FParse::Param(FCommandLine::Get(), TEXT("devmode-shots")))
    {
        return;
    }
    // Deterministic file name (no frame suffix) so runs overwrite and the
    // golden gate can match by exact name.
    FScreenshotRequest::RequestScreenshot(
        FString::Printf(TEXT("DevBot_%s_%s"), *ScenarioName, *Tag), true, false);
    UE_LOG(LogTemp, Log, TEXT("DEVMODE_BOT_SHOT %s_%s"), *ScenarioName, *Tag);
}

void UDevModeBotBrain::Finish(bool bPass, const FString& Reason)
{
    if (!bRunning)
    {
        return;
    }
    const bool bOk = bPass && Failures == 0;
    Stop();
    UE_LOG(LogTemp, Warning, TEXT("%s_RESULT %s %s kills=%d failures=%d reason=%s"),
        *TombstonePrefix, *ScenarioName, bOk ? TEXT("PASS") : TEXT("FAIL"), Kills, Failures, *Reason);

    // Auto-run paths (-devmode=<scenario>, -bot-live=<scenario>) quit so CI
    // gates on the tombstone; manual console runs stay in the world.
    if (bAutoQuitAtFinish || (GameInstance && GameInstance->bDevModeAutoQuit))
    {
        GameInstance->bDevModeAutoQuit = false;
        GameInstance->DevModeAutoScenario.Empty();
        if (UWorld* World = BrainWorld.Get())
        {
            if (GEngine)
            {
                GEngine->Exec(World, TEXT("quit"));
            }
        }
    }
}

// ---- Phase A helpers ----

ADroppedItemActor* UDevModeBotBrain::FindNearestDroppedItem(float MaxDistance) const
{
    UWorld* World = BrainWorld.Get();
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!World || !Player) { return nullptr; }

    ADroppedItemActor* Best = nullptr;
    float BestDist = MaxDistance;
    const FVector At = Player->GetActorLocation();
    for (TActorIterator<ADroppedItemActor> It(World); It; ++It)
    {
        ADroppedItemActor* Drop = *It;
        if (!IsValid(Drop)) { continue; }
        const float D = Dist2D(At, Drop->GetActorLocation());
        if (D < BestDist) { BestDist = D; Best = Drop; }
    }
    return Best;
}

ABasicMOB* UDevModeBotBrain::ResolveCorpseActor() const
{
    // Our victim if its corpse actor still stands (usual case)...
    if (TargetMob.IsValid() && !TargetMob->IsActorBeingDestroyed())
    {
        return TargetMob.Get();
    }
    // ...else the nearest corpse — the bot stands on its kill, so the
    // nearest one is ours unless the server already cleaned it up.
    if (GameInstance && GameInstance->GetHarvestManager())
    {
        UHarvestManager* Harvest = GameInstance->GetHarvestManager();
        if (ABasicMOB* Harvestable = Harvest->GetNearestHarvestableCorpse(600.0f))
        {
            return Harvestable;
        }
        return Harvest->GetNearestHarvestedCorpse(600.0f);
    }
    return nullptr;
}

bool UDevModeBotBrain::SpawnStubDrop(ABasicPlayer* Player)
{
    if (!GameInstance || !Player) { return false; }
    UItemManager* ItemManager = GameInstance->GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT stub drop failed: no ItemManager"));
        return false;
    }
    if (GameInstance->GetDroppedItemActorClass() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT stub drop failed: no DroppedItemActorClass"));
        return false;
    }

    // Dev-range UID: never collides with seeded ids (101..301) or mob UIDs.
    FDroppedItemStruct Drop;
    Drop.uid = LootDropUID;
    Drop.itemId = LootItemTemplateId;
    Drop.item.id = LootItemTemplateId;
    Drop.item.name = TEXT("Health Potion");
    Drop.item.slug = TEXT("health_potion");
    Drop.quantity = 1;
    Drop.canBePickedUp = true;
    const FVector At = LastKillLocation;
    Drop.position.positionX = At.X;
    Drop.position.positionY = At.Y;
    Drop.position.positionZ = At.Z + 50.0;

    ADroppedItemActor* Actor = ItemManager->SpawnDroppedItem(Drop);
    if (!IsValid(Actor))
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT stub drop failed: SpawnDroppedItem returned null"));
        return false;
    }
    LootDropActor = Actor;
    UE_LOG(LogTemp, Warning, TEXT("%s_LOOT_STUB uid=%d 'Health Potion' at corpse (offline drop, real spawn path)"),
        *TombstonePrefix, LootDropUID);
    return true;
}

void UDevModeBotBrain::ResolveStubLoot(ABasicPlayer* Player)
{
    if (!GameInstance || !Player) { return; }
    // Offline: no server confirm exists, so the pickup resolves through the
    // same local calls the server path ends in (inventory add + pickup
    // broadcast + world actor removal). Walking here and the asserts are
    // real; only the server round-trip is stubbed.
    FInventoryItemStruct NewItem;
    NewItem.id = LootDropUID;
    NewItem.itemId = LootItemTemplateId;
    NewItem.quantity = 1;
    NewItem.name = TEXT("Health Potion");
    NewItem.slug = TEXT("health_potion");
    NewItem.itemSlug = TEXT("health_potion");
    if (UInventoryManager* Inv = GameInstance->GetInventoryManager())
    {
        Inv->AddItemToLocalInventory(NewItem);
    }
    FItemBaseStruct Base;
    Base.id = LootItemTemplateId;
    Base.name = TEXT("Health Potion");
    Base.slug = TEXT("health_potion");
    if (UItemManager* Items = GameInstance->GetItemManager())
    {
        Items->ProcessItemPickup(Base);
    }
    if (ADroppedItemActor* Drop = LootDropActor.Get())
    {
        Drop->PlayPickupEffect();
        Drop->Destroy();
    }
    LootDropActor.Reset();
    bPickupRequested = true;
    UE_LOG(LogTemp, Warning, TEXT("%s_LOOT_STUB resolved uid=%d into inventory (offline pickup)"),
        *TombstonePrefix, LootDropUID);
}

void UDevModeBotBrain::ApplyStubDeath(ABasicPlayer* Player)
{
    if (!Player) { return; }
    // Offline: no server damage exists. The death goes through the real
    // SetDead(true) path (death anim, movement lock, death screen) — the
    // same call server-driven death takes. Only the damage is stubbed.
    Player->SetPlayerCurrentHPPoints(0);
    Player->SetDead_Implementation(true);
    UE_LOG(LogTemp, Warning, TEXT("%s_DEATH_STUB player killed offline (real SetDead path)"),
        *TombstonePrefix);
}

void UDevModeBotBrain::ApplyStubRevive(ABasicPlayer* Player)
{
    if (!Player) { return; }
    // Offline: no server respawnResult exists. Revive through the real
    // SetDead(false) path (movement restore, screen hide, OnRevive) at the
    // run spawn with full HP — the same observable state a server respawn
    // produces. Only the packet is stubbed.
    const int32 MaxHP = Player->GetMaxHealth_Implementation();
    Player->SetActorLocation(RespawnPoint);
    Player->SetDead_Implementation(false);
    Player->SetPlayerCurrentHPPoints(MaxHP > 0 ? MaxHP : 1000);
    UE_LOG(LogTemp, Warning, TEXT("%s_RESPAWN_STUB player revived at spawn hp=%d (real SetDead path)"),
        *TombstonePrefix, Player->GetPlayerCurrentHPPoints());
}

int32 UDevModeBotBrain::CountInventoryItems() const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        return GameInstance->GetInventoryManager()->GetInventory().items.Num();
    }
    return -1;
}

int32 UDevModeBotBrain::SumInventoryQuantities() const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        int32 Total = 0;
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            Total += Item.quantity;
        }
        return Total;
    }
    return -1;
}

bool UDevModeBotBrain::IsDeathScreenVisible() const
{
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    UUIManager* UI = Player ? Player->GetUIManager() : nullptr;
    return UI ? UI->IsDeathScreenVisible() : false;
}

// ---- Phase B helpers ----

int32 UDevModeBotBrain::GetGold() const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        return GameInstance->GetInventoryManager()->GetInventory().gold;
    }
    return -1;
}

int32 UDevModeBotBrain::FindSellableInstanceId(int32& OutQty) const
{
    OutQty = 0;
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        // Junk = stackable, non-equipped, non-durable, non-quest, non-currency
        // row. The row survives the sale (qty - 1), so the assert reads qty
        // exactly. Currency rows (gold_coin, qty == balance) must never be
        // sold — the server eats the coin without credit (seen live).
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.quantity > 1 && !Item.is_equipped && !Item.isDurable && !Item.isQuestItem
                && !Item.itemTypeSlug.Equals(TEXT("currency"), ESearchCase::IgnoreCase))
            {
                OutQty = Item.quantity;
                return Item.id;
            }
        }
    }
    return 0;
}

int32 UDevModeBotBrain::FindDamagedInstanceId(int32& OutMaxDur) const
{
    OutMaxDur = 0;
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.durabilityMax > 0 && Item.durabilityCurrent < Item.durabilityMax)
            {
                OutMaxDur = Item.durabilityMax;
                return Item.id;
            }
        }
    }
    return 0;
}

int32 UDevModeBotBrain::GetQtyByInstance(int32 InstanceId) const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.id == InstanceId)
            {
                return Item.quantity;
            }
        }
    }
    return -1;
}

int32 UDevModeBotBrain::GetDurabilityByInstance(int32 InstanceId) const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.id == InstanceId)
            {
                return Item.durabilityCurrent;
            }
        }
    }
    return -1;
}

// ---- Phase C helpers ----

bool UDevModeBotBrain::HasItemTemplate(int32 TemplateItemId) const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.itemId == TemplateItemId && Item.quantity > 0)
            {
                return true;
            }
        }
    }
    return false;
}

ABasicPlayer* UDevModeBotBrain::FindPeerActor() const
{
    if (!GameInstance)
    {
        return nullptr;
    }
    // This client owns exactly one entry: any other live actor is the peer.
    const int32 OwnId = GameInstance->GetCurrentClientID();
    for (const TPair<int32, ABasicPlayer*>& E : GameInstance->SpawnedPlayers)
    {
        if (E.Key != OwnId && IsValid(E.Value) && !E.Value->IsActorBeingDestroyed())
        {
            return E.Value;
        }
    }
    return nullptr;
}

// ---- Phase D helpers ----

bool UDevModeBotBrain::HasActiveEffect(const FString& Slug) const
{
    if (GameInstance && GameInstance->GetPlayerStatsManager())
    {
        for (const FActiveEffectEntry& E : GameInstance->GetPlayerStatsManager()->GetCachedStats().activeEffects)
        {
            if (E.slug.Equals(Slug, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
    }
    return false;
}

int32 UDevModeBotBrain::FindEquippableMainHand(FString& OutSlug) const
{
    OutSlug.Empty();
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.isEquippable && !Item.is_equipped
                && Item.equipSlotSlug.Equals(TEXT("main_hand"), ESearchCase::IgnoreCase))
            {
                OutSlug = Item.itemSlug;
                return Item.id;
            }
        }
    }
    return 0;
}

int32 UDevModeBotBrain::GetProgressionLevel() const
{
    if (GameInstance && GameInstance->GetExperienceManager())
    {
        return GameInstance->GetExperienceManager()
            ->GetCharacterProgression(GameInstance->GetCurrentCharacterID()).currentLevel;
    }
    return 0;
}

int32 UDevModeBotBrain::GetProgressionExp() const
{
    if (GameInstance && GameInstance->GetExperienceManager())
    {
        return GameInstance->GetExperienceManager()
            ->GetCharacterProgression(GameInstance->GetCurrentCharacterID()).totalExperience;
    }
    return 0;
}

bool UDevModeBotBrain::EnsureDialogueOpen(int32 NpcId, double Now)
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return false;
    }
    if (Dlg->IsDialogueActive())
    {
        return true;
    }
    // (Re)open, throttled: covers the first open and server-side closes
    // (TTL, end-nodes) mid-leg.
    if (!bDialogueRequested || Now - LastDialogueRequestTime > 5.0)
    {
        Dlg->OpenDialogue(NpcId);
        bDialogueRequested = true;
        LastDialogueRequestTime = Now;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT dialogue opened npc=%d"), NpcId);
    }
    return false;
}

int32 UDevModeBotBrain::FindGoalEdge(bool bWantTurnin) const
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return 0;
    }
    for (const FDialogueChoice& Choice : Dlg->GetCurrentNode().choices)
    {
        const bool bGoal = bWantTurnin ? Choice.bHasTurnInPreview : Choice.bHasQuestPreview;
        if (bGoal && Choice.conditionMet)
        {
            return Choice.edgeId;
        }
    }
    return 0;
}

int32 UDevModeBotBrain::FindAdvanceEdge() const
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return 0;
    }
    // Neutral hubs (continue/back): first met condition that is NOT a
    // farewell-class edge. Goal edges have priority in the caller, so this
    // never steals an accept/turn-in.
    for (const FDialogueChoice& Choice : Dlg->GetCurrentNode().choices)
    {
        if (!Choice.conditionMet)
        {
            continue;
        }
        const FString Key = Choice.clientChoiceKey.ToLower();
        if (Key.Contains(TEXT("farewell")) || Key.Contains(TEXT("bye")) || Key.Contains(TEXT("leave"))
            || Key.Contains(TEXT("goodbye")) || Key.Contains(TEXT("close")) || Key.Contains(TEXT("exit"))
            || Key.Contains(TEXT("cancel")))
        {
            continue;
        }
        return Choice.edgeId;
    }
    return 0;
}

void UDevModeBotBrain::QuestLootDone(double Now)
{
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (Kills >= TargetKills)
    {
        Step = EBotStep::QuestTurnin;
        QuestLegStartTime = Now;
        bDialogueRequested = false;
        bChoiceSent = false;
        DialogueClicks = 0;
        LastAdvanceNodeId = 0;
        GoldBeforeReward = GetGold();
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT objectives done, returning to turn in"));
        return;
    }
    // More kills to go: this corpse is resolved (looted or empty), hunt on.
    TargetMob.Reset();
    bHarvestRequested = false;
    bInspectRequested = false;
    bPickupRequested = false;
    CorpseUID = 0;
    Step = EBotStep::GotoTarget;
    StepStartTime = Now;
    if (Player)
    {
        LastChasePos = Player->GetActorLocation();
        LastChaseProgressTime = Now;
    }
    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT corpse resolved, hunting next (%d/%d)"), Kills, TargetKills);
}
