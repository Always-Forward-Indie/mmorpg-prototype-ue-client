#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "UI/UIManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "CrashDiagnostics.h"

void UCombatSystemManager::ProcessSkillInitiation(const FSkillInitiationData& SkillData)
{
    LogCombatEvent("Skill Initiation", 
        FString::Printf(TEXT("Skill: %s, Caster: %d (%s), Target: %d (%s), cooldownMs: %d, gcdMs: %d"), 
            *SkillData.skillName, SkillData.casterId, *SkillData.casterTypeString,
            SkillData.targetId, *SkillData.targetTypeString,
            SkillData.cooldownMs, SkillData.gcdMs));

    // Flush any pending results from a previous cast BEFORE processing this new
    // initiation. This applies the old damage at the correct time (now, since the
    // old animation is being superseded) instead of losing it or having two results
    // queued that would both fire at the same HitPoint (double damage).
    FlushAllPendingForCaster(SkillData.casterId);

    // Handle skill initiation in PlayerSkillManager for cooldown + GCD management
    if (GameInstance)
    {
        UPlayerSkillManager* PlayerSkillManager = GameInstance->GetPlayerSkillManager();
        if (PlayerSkillManager)
        {
            PlayerSkillManager->HandleSkillInitiation(SkillData.skillSlug, SkillData.casterId,
                SkillData.cooldownMs, SkillData.gcdMs);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: PlayerSkillManager not available"));
        }
    }

    // S1a: server no longer sends display strings (skillName/animationName).
    // Resolve them from the local skill catalog; keep packet values as override.
    FString SkillNameForUI = SkillData.skillName;
    FString AnimNameForUI = SkillData.animationName;
    if ((SkillNameForUI.IsEmpty() || AnimNameForUI.IsEmpty()) && GameInstance)
    {
        if (USkillDefinitionRepository* SkillRepo = GameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = SkillRepo->GetDefinition(SkillData.skillSlug);
            if (SkillNameForUI.IsEmpty() && !Def.displayName.IsEmpty())
                SkillNameForUI = Def.displayName.ToString();
            if (AnimNameForUI.IsEmpty() && !Def.animationName.IsEmpty())
                AnimNameForUI = Def.animationName;
        }
    }

    // Find caster
    ECasterType CasterType = MapNetCasterType(SkillData.casterType, SkillData.casterTypeString);
    TScriptInterface<ICombatable> Caster = FindCombatableById(SkillData.casterId, CasterType);

    UE_LOG(LogTemp, Warning, TEXT("INIT: casterId=%d netType=%d(%s) mapped=%d key=%s"),
        SkillData.casterId, SkillData.casterType, *SkillData.casterTypeString,
        (int32)CasterType, *CreateCombatableKey(SkillData.casterId, CasterType));

    if (Caster.GetInterface() && Caster.GetObject() && IsValid(Caster.GetObject()))
    {
        UObject* CasterObject = Caster.GetObject();

        // ShowCastBar must run BEFORE PlaySkillAnimation so that BasicPlayer::CurrentCastTime
        // is set when StartAttack() reads it to choose the phase-based or classic anim path.
        if (SkillData.castTime > 0.0f)
        {
            ICombatable::Execute_ShowCastBar(CasterObject, SkillData.castTime, SkillNameForUI);
        }

        // Play animation on caster (animationDuration drives PlayRate)
        ICombatable::Execute_PlaySkillAnimation(CasterObject, AnimNameForUI, SkillData.skillSlug, SkillData.animationDuration);

		// Set target if specified
		if (SkillData.targetId > 0)
		{
			ECasterType TargetType = MapNetCasterType(SkillData.targetType, SkillData.targetTypeString);
			ICombatable::Execute_SetTarget(CasterObject, SkillData.targetId, TargetType);
			ICombatable::Execute_SetIsAggressiveState(CasterObject, SkillData.targetId, TargetType, SkillData.skillEffectType == ESkillEffectType::Damage);
		}
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Could not find caster %d (%s) for skill initiation"),
            SkillData.casterId, *SkillData.casterTypeString);
    }

    // Broadcast event
    OnSkillInitiated.Broadcast(SkillData);
}

ECasterType UCombatSystemManager::MapNetCasterType(int32 Net, const FString& Str)
{
    // String-based mapping first (older server packets); int enum is authoritative
    // since protocol S1a and is the fallback when the string is absent.
    if (Str.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase)) return ECasterType::Player;
    if (Str.Equals(TEXT("MOB"),    ESearchCase::IgnoreCase)) return ECasterType::Mob;
    if (Str.Equals(TEXT("NPC"),    ESearchCase::IgnoreCase)) return ECasterType::NPC;
    if (Str.Equals(TEXT("SELF"),   ESearchCase::IgnoreCase)) return ECasterType::Self;
    if (Str.Equals(TEXT("AREA"),   ESearchCase::IgnoreCase)) return ECasterType::Area;

    // Fallback: enum values now match the server protocol directly (SELF=1..NPC=6)
    if (Net >= 1 && Net <= 6)
        return static_cast<ECasterType>(Net);

    return ECasterType::None;
}

