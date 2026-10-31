#include "DevMode/DevModeBotBrain.h"
#include "Prototyping.h"
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
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "HighResScreenshot.h"
#include "Misc/Parse.h"
#include "DevMode/BotBrainHelpers.h"

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

bool UDevModeBotBrain::BuildNavPath(ABasicPlayer* Player, const FVector& Dest, double Now)
{
    bNavPathValid = false;
    NavPathPoints.Reset();
    NavPathIndex = 0;
    NavDetourCount = 0;
    NavFailStage = 1;
    if (!Player)
    {
        return false;
    }
    UWorld* World = BrainWorld.Get();
    if (!World)
    {
        return false;
    }
    NavFailStage = 2;
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys)
    {
        return false;
    }
    const FVector PlayerAt = Player->GetActorLocation();
    // Agent sized like the pawn's capsule; falls back to the small
    // agent class when the capsule is unavailable or unmatched (lookup
    // can reject an agent the mesh was not built for) — a cascade.
    ANavigationData* NavData = nullptr;
    if (const UCapsuleComponent* Cap = Player->GetCapsuleComponent())
    {
        FNavAgentProperties Props;
        Props.AgentRadius = Cap->GetScaledCapsuleRadius();
        Props.AgentHeight = Cap->GetScaledCapsuleHalfHeight() * 2.0f;
        NavData = NavSys->GetNavDataForProps(Props, PlayerAt);
    }
    if (!NavData)
    {
        FNavAgentProperties Small;
        Small.AgentRadius = 30.0f;
        Small.AgentHeight = 176.0f;
        NavData = NavSys->GetNavDataForProps(Small, PlayerAt);
    }
    NavFailStage = 3;
    if (!NavData)
    {
        if (!bNavLoggedMissing)
        {
            bNavLoggedMissing = true;
            const ANavigationData* DefaultNav = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT no navmesh data (stage 3) default=[%s] at (%.0f,%.0f,%.0f)"),
                DefaultNav ? *DefaultNav->GetName() : TEXT("(none)"), PlayerAt.X, PlayerAt.Y, PlayerAt.Z);
        }
        return false;
    }
    FPathFindingQuery Query(Player, *NavData, PlayerAt, Dest);
    const FPathFindingResult Result = NavSys->FindPathSync(Query);
    NavFailStage = 4;
    if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial())
    {
        return false;
    }
    TArray<FVector> RawPoints;
    for (const FNavPathPoint& P : Result.Path->GetPathPoints())
    {
        RawPoints.Add(P.Location);
    }
    // Drop points already behind us (path start ~= current position).
    for (const FVector& P : RawPoints)
    {
        if (Dist2D(PlayerAt, P) > NavPointRadius)
        {
            NavPathPoints.Add(P);
        }
    }
    if (NavPathPoints.Num() == 0)
    {
        NavFailStage = 5;
        return false;
    }
    NavPathDest = Dest;
    NavPathIndex = 0;
    LastNavRepathSec = Now;
    bNavPathValid = true;
    if (!bNavLoggedActive)
    {
        bNavLoggedActive = true;
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT navmesh active (%d points on %s)"), NavPathPoints.Num(), NavData ? *NavData->GetName() : TEXT("?"));
    }
    return true;
}

void UDevModeBotBrain::NavSteerTarget(ABasicPlayer* Player, const FVector& Dest, double Now, FVector& OutTarget)
{
    OutTarget = Dest;
    if (!Player)
    {
        return;
    }
    UWorld* World = BrainWorld.Get();
    if (!World)
    {
        return;
    }
    const FVector PlayerAt = Player->GetActorLocation();
    const bool bDestDrifted = !bNavPathValid || Dist2D(NavPathDest, Dest) > NavRepathMinDrift;
    // Off-route (knockback, far wander): the remaining points no longer
    // describe where we are — rebuild instead of cutting across walls.
    bool bOffRoute = false;
    if (bNavPathValid && NavPathIndex < NavPathPoints.Num())
    {
        float Nearest = FLT_MAX;
        for (int32 i = NavPathIndex; i < NavPathPoints.Num(); ++i)
        {
            Nearest = FMath::Min(Nearest, Dist2D(PlayerAt, NavPathPoints[i]));
        }
        bOffRoute = Nearest > 600.0f;
    }
    if (!bNavPathValid || bDestDrifted || bOffRoute
        || (Dist2D(NavPathDest, Dest) > 50.0f && Now - LastNavRepathSec > NavRepathIntervalSec))
    {
        NavFailStage = 0;
        if (!BuildNavPath(Player, Dest, Now))
        {
            // Stage 3 logs itself (with the available mesh list).
            if (NavFailStage != 3 && !bNavLoggedMissing)
            {
                bNavLoggedMissing = true;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT no navmesh path (stage %d) — direct steering"), NavFailStage);
            }
            return; // OutTarget stays Dest: direct steering, status quo.
        }
    }
    // Consume reached waypoints.
    while (NavPathIndex < NavPathPoints.Num()
        && Dist2D(PlayerAt, NavPathPoints[NavPathIndex]) < NavPointRadius)
    {
        ++NavPathIndex;
    }
    if (NavPathIndex >= NavPathPoints.Num())
    {
        // Path consumed but the leg hasn't arrived (drift since the build):
        // rebuild once, else steer direct this tick (arrival check upstream
        // still bounds the leg).
        if (BuildNavPath(Player, Dest, Now) && NavPathIndex < NavPathPoints.Num())
        {
            OutTarget = NavPathPoints[NavPathIndex];
        }
        return;
    }
    // Final approach goes straight at Dest so the leg's own arrival radius
    // (often tighter than a waypoint) decides completion.
    if (NavPathIndex == NavPathPoints.Num() - 1)
    {
        OutTarget = Dest;
        return;
    }
    OutTarget = NavPathPoints[NavPathIndex];
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

    // Navmesh first: walk path points toward Dest instead of pushing blindly
    // into walls. Falls back to Dest itself (direct steering, status quo)
    // when no usable path exists.
    FVector SteerTarget = Dest;
    NavSteerTarget(Player, Dest, Now, SteerTarget);

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
        FVector Dir = SteerTarget - PlayerAt;
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
        // Intent is published here and pumped per-frame by SteerTick: input
        // applied only in this 10Hz tick would crawl at high fps (input is
        // consumed every rendered frame). Immediate apply keeps this tick's
        // behavior identical; the ticker sustains it between ticks.
        FVector Dir = SteerTarget - PlayerAt;
        Dir.Z = 0.0f;
        if (!Dir.Normalize()) { return true; }
        SteerDest = SteerTarget;
        LastSteerTime = Now;
        bSteerIntent = true;
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
            // (positionCorrection), but teleports stick — stale pre-teleport
            // moves die in the commit grace window (commitTeleportPosition).
            LastChasePos = PlayerAt;
            if (bNavPathValid && NavPathIndex < NavPathPoints.Num())
            {
                // Local avoidance: the global corridor threads a wall the
                // sweep can't cross (repaths return the same corridor).
                // Insert an alternating, widening sidestep ahead of the
                // carrot so the route bends around it. Bounded by the leg
                // timeout — a truly blocked leg still fails honestly.
                ++NavDetourCount;
                FVector Fwd = Dest - PlayerAt;
                Fwd.Z = 0.0f;
                if (Fwd.Normalize())
                {
                    const float SideSign = (NavDetourCount % 2 == 0) ? 1.0f : -1.0f;
                    const float SideDist = 300.0f * ((NavDetourCount + 1) / 2);
                    const FVector Detour = PlayerAt + Fwd * 200.0f + FVector(-Fwd.Y, Fwd.X, 0.0f) * SideSign * SideDist;
                    NavPathPoints.Insert(Detour, NavPathIndex);
                    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_STUCK %s (dist %.0f, detour %d %.0fuu) from (%.0f,%.0f,%.0f) blocked=%d on='%s'"),
                        Label, Dist2D(PlayerAt, Dest), NavDetourCount, SideDist, PlayerAt.X, PlayerAt.Y, PlayerAt.Z,
                        bLastMoveBlocked ? 1 : 0, *LastBlockingActor);
                }
            }
            else
            {
                // No usable path (direct steering): nothing to bend.
                // Invalidate so the next tick repaths or steers direct.
                bNavPathValid = false;
                NavPathPoints.Reset();
                NavPathIndex = 0;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_STUCK %s (dist %.0f, no teleport on live) from (%.0f,%.0f,%.0f) blocked=%d on='%s'"),
                    Label, Dist2D(PlayerAt, Dest), PlayerAt.X, PlayerAt.Y, PlayerAt.Z,
                    bLastMoveBlocked ? 1 : 0, *LastBlockingActor);
            }
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
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_FALLBACK teleport %s (dist %.0f) pawn='%s' ctrl='%s' locallycontrolled=%d maxwalk=%.0f compregistered=%d isother=%d from (%.0f,%.0f,%.0f) vel=%.0f mode=%d"),
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
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_INPUT pendmm=%d lastmm=%d"),
                FMath::RoundToInt(MoveComp->GetPendingInputVector().Size() * 1000.0),
                FMath::RoundToInt(MoveComp->GetLastInputVector().Size() * 1000.0));
        }
        }
    }
    return false;
}

bool UDevModeBotBrain::SteerTick(float)
{
    if (!bRunning)
    {
        return false;
    }
    // Queued BotShot pump (see BotShot): issue the parked request the moment
    // the single-slot screenshot pipe is free. Runs even with no steering
    // intent (death/respawn checkpoints steer nothing).
    if (!PendingShotName.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested())
    {
        FScreenshotRequest::RequestScreenshot(PendingShotName, true, false);
        UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_BOT_SHOT_FLUSHED %s"), *PendingShotName);
        PendingShotName.Empty();
    }
    if (!bSteerIntent)
    {
        return true;
    }
    // Intent older than 3 BrainTicks with no SteerToward refresh: the leg
    // moved on (arrival, step change) or the world traveled — stop pushing.
    if (FPlatformTime::Seconds() - LastSteerTime > 0.3)
    {
        bSteerIntent = false;
        return true;
    }
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!Player || Player->IsActorBeingDestroyed())
    {
        return true;
    }
    FVector Dir = SteerDest - Player->GetActorLocation();
    Dir.Z = 0.0f;
    if (Dir.Normalize())
    {
        Player->AddMovementInput(Dir, 1.0f);
    }
    return true;
}

