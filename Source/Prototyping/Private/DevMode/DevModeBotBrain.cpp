#include "DevMode/DevModeBotBrain.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Items/HarvestManager.h"
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
            TargetMob = FindNearestAliveMob();
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
            if (bDeathPhase)
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
            ResolveStubLoot(Player);
            const int32 Have = CountInventoryItems();
            const int32 HaveQty = SumInventoryQuantities();
            BotAssert(Have == InitialInvCount + 1, TEXT("loot-inv-plus-one"), Have, InitialInvCount + 1);
            BotAssert(HaveQty == InitialInvQty + 1, TEXT("loot-qty-plus-one"), HaveQty, InitialInvQty + 1);
            BotShot(TEXT("loot"));
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
        // Phase 1: harvest the corpse (server progress round-trip). Already-
        // harvested corpses skip straight to inspect.
        if (Corpse->CanBeHarvested())
        {
            if (!bHarvestRequested)
            {
                Player->SetLockedTarget(Corpse);
                Harvest->TryHarvestSpecificCorpse(Corpse);
                bHarvestRequested = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT harvest requested on corpse uid=%d"), CorpseUID);
            }
            if (Now - LootLegStartTime > LootTimeoutSec)
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
        // reported loudly rather than timing out.
        if (Harvest->IsCorpseEmpty(CorpseUID))
        {
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
