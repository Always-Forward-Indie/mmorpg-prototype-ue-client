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
        // Intent is published here and pumped per-frame by SteerTick: input
        // applied only in this 10Hz tick would crawl at high fps (input is
        // consumed every rendered frame). Immediate apply keeps this tick's
        // behavior identical; the ticker sustains it between ticks.
        FVector Dir = Dest - PlayerAt;
        Dir.Z = 0.0f;
        if (!Dir.Normalize()) { return true; }
        SteerDest = Dest;
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
        UE_LOG(LogTemp, Log, TEXT("DEVMODE_BOT_SHOT_FLUSHED %s"), *PendingShotName);
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

