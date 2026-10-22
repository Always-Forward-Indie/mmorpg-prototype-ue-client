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

bool UDevModeBotBrain::Tick_SkillWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Honest walk to the trainer (shop range is server-side).
        if (SteerToward(Player, SkillNpcPos, 150.0f, Now, TEXT("trainer")))
        {
            Step = EBotStep::SkillLearn;
            CharLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at trainer npc=%d, opening skill shop"), SkillNpcId);
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            Finish(false, TEXT("skills-unreachable"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_SkillLearn(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        USkillShopManager* Shop = GameInstance->GetSkillShopManager();
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!Shop || !PSM)
        {
            Finish(false, TEXT("no-skills"));
            return true;
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
            return true;
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
            return true;
        }
        BotAssert(true, TEXT("skill-learned-power"), 1, 1);
        BotAssert(true, TEXT("skill-learned-cry"), 1, 1);
        BotShot(TEXT("learned"));
        Step = EBotStep::SkillCast;
        CharLegStartTime = Now;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT skills learned, hunting cast target"));
        return true;
}

bool UDevModeBotBrain::Tick_SkillCast(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!PSM)
        {
            Finish(false, TEXT("no-skills"));
            return true;
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
                    return true;
                }
                ChaseTargetUID = 0;
                bCastSteerInit = false;
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
                return true;
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                BotAssert(bDamageSeen, TEXT("skill-damage"), bDamageSeen ? 1 : 0, 1);
                BotAssert(bCooldownSeen, TEXT("skill-cooldown"), bCooldownSeen ? 1 : 0, 1);
                BotAssert(bManaSeen, TEXT("skill-mana"), bManaSeen ? 1 : 0, 1);
                Finish(false, TEXT("cast-timeout"));
            }
            // Fizzle retry: zero confirms 15s after the send means the server
            // rejected the initiation (stale positions on a drifting player /
            // wandering mob read "out of range") — re-approach and try a
            // fresh cast instead of idling to the leg timeout. The overall
            // CharLegStartTime budget still bounds the leg.
            if (!bDamageSeen && !bCooldownSeen && !bManaSeen && Now - CastSentTime > 15.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT cast fizzled (no confirm in 15s), re-approaching"));
                bCastSent = false;
                TargetMob.Reset();
                bCastSteerInit = false;
                CastInRangeSince = 0.0;
            }
            return true;
        }
        if (Dist2D(PlayerAt, Target->GetActorLocation()) > CastRange * CastArriveFrac)
        {
            SteerToward(Player, Target->GetActorLocation(), CastRange * CastArriveFrac, Now, *Target->GetMobName());
            const float TargetDist = Dist2D(PlayerAt, Target->GetActorLocation());
            if (!bCastSteerInit)
            {
                bCastSteerInit = true;
                ChaseCheckTime = Now;
                ChaseCheckDist = TargetDist;
            }
            // Same safeguard as the death leg: no distance gain in the
            // stall window means a wall the server won't let us cross —
            // exclude this pick and repick (fresh clock) instead of
            // hanging to the leg timeout. Symmetric case: the pick is
            // fleeing faster than the bot runs (gain deeply negative) —
            // uncatchable, every cast reads out of range server-side.
            if (Now - ChaseCheckTime > ChaseStallSec)
            {
                const float Gain = ChaseCheckDist - TargetDist;
                if (Gain < -ChaseFleeLoss)
                {
                    ExcludedKillerUIDs.Add(TargetUid);
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT cast target fleeing (gain %.0f in %.0fs) — excluding uid=%d, repicking"),
                        Gain, ChaseStallSec, TargetUid);
                    TargetMob.Reset();
                    bCastSteerInit = false;
                    CastInRangeSince = 0.0;
                    return true;
                }
                if (Gain < ChaseMinGain)
                {
                    ExcludedKillerUIDs.Add(TargetUid);
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT cast target unreachable (gain %.0f in %.0fs) — excluding uid=%d, repicking"),
                        ChaseCheckDist - TargetDist, ChaseStallSec, TargetUid);
                    TargetMob.Reset();
                    bCastSteerInit = false;
                    CastInRangeSince = 0.0;
                    return true;
                }
                ChaseCheckTime = Now;
                ChaseCheckDist = TargetDist;
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                Finish(false, TEXT("cast-unreachable"));
            }
            CastInRangeSince = 0.0;
            return true;
        }
        // Hold inside cast range briefly so the server's tracked positions
        // converge before committing (same settle pattern as chat/rendezvous:
        // a throttled headless client outruns the server's movement view,
        // which then reads "out of range" on an actually close cast).
        if (CastInRangeSince <= 0.0)
        {
            CastInRangeSince = Now;
            return true;
        }
        if (Now - CastInRangeSince < 2.0)
        {
            SteerToward(Player, Target->GetActorLocation(), CastRange * CastArriveFrac, Now, *Target->GetMobName());
            return true;
        }
        if (Player->GetLockedTarget() != Target)
        {
            Player->SetLockedTarget(Target);
        }
        MobHpBefore = Target->GetMOBCurrentHealth();
        ManaBefore = Player->GetPlayerCurrentMPPoints();
        PSM->TryCastSkill(TEXT("power_slash"), TargetUid, ECasterType::Mob);
        bCastSent = true;
        CastSentTime = Now;
        bDamageSeen = false;
        bCooldownSeen = false;
        bManaSeen = false;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT power_slash cast on uid=%d (mobhp=%d mana=%d)"),
            TargetUid, MobHpBefore, ManaBefore);
        return true;
}

bool UDevModeBotBrain::Tick_SkillBuff(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!PSM)
        {
            Finish(false, TEXT("no-skills"));
            return true;
        }
        if (!bBuffSent)
        {
            PSM->TryCastSkill(TEXT("battle_cry"), 0, ECasterType::Player);
            bBuffSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT battle_cry cast (self)"));
            return true;
        }
        // The buff lands as a server stats_update activeEffects entry —
        // the same snapshot ActiveEffectsWidget renders.
        if (HasActiveEffect(TEXT("battle_cry_phys_atk")))
        {
            BotAssert(true, TEXT("buff-active"), 1, 1);
            BotShot(TEXT("buff"));
            Finish(Failures == 0, Failures == 0 ? TEXT("skills-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("buff-active"), 0, 1);
            Finish(false, TEXT("buff-timeout"));
        }
        return true;
}
