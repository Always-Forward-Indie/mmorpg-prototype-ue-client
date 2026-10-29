#include "Gameplay/Combat/HealingEffectHandler.h"
#include "Prototyping.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Players/BasicPlayer.h"

UHealingEffectHandler::UHealingEffectHandler()
{
}

bool UHealingEffectHandler::CanHandle_Implementation(ESkillEffectType EffectType) const
{
    return EffectType == ESkillEffectType::Healing;
}

void UHealingEffectHandler::ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        UE_LOG(LogGameplay, Error, TEXT("HealingEffectHandler: Target interface is null or invalid"));
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // Extract healing value
    int32 FinalHealing = SkillResult.healing;

    // Handle critical heals
    if (SkillResult.isCritical)
    {
        ProcessCriticalHeal(SkillResult, TargetObject);
    }

    // Use server-authoritative finalTargetHealth instead of local calculation.
    // The server already computed the correct HP after healing (clamped to max).
    // Single-authority rule for the LOCAL player (same as DamageEffectHandler):
    // vitals are owned exclusively by stats_update; a deferred heal result
    // carries an OLD finalTargetHealth and would yank the bar. Numbers still
    // display from the factual heal amounts via the edge cases below.
    int32 OldHealth = ICombatable::Execute_GetCurrentHealth(TargetObject);
    int32 OldMana   = ICombatable::Execute_GetCurrentMana(TargetObject);
    bool bSkipVitalsForLocalPlayer = false;
    if (ABasicPlayer* TargetPlayer = Cast<ABasicPlayer>(TargetObject))
    {
        bSkipVitalsForLocalPlayer = !TargetPlayer->GetIsOtherClient();
    }
    int32 NewHealth = SkillResult.finalTargetHealth;
    if (bSkipVitalsForLocalPlayer)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("HealingEffectHandler: skipping combat HP set for local player %d (stats_update owns vitals; result HP=%d)"),
            ICombatable::Execute_GetActorId(TargetObject), SkillResult.finalTargetHealth);
        NewHealth = OldHealth;
    }
    else
    {
        ICombatable::Execute_SetCurrentHealth(TargetObject, NewHealth);
    }

    // Only overwrite mana if the server sent a meaningful value; 0 means the field
    // was not included in this packet (e.g. HP-only skill packets) and we must not
    // zero out the player's current mana.
    if (SkillResult.finalTargetMana > 0 && !bSkipVitalsForLocalPlayer)
    {
        ICombatable::Execute_SetCurrentMana(TargetObject, SkillResult.finalTargetMana);
    }

    // Calculate actual healing done for display (in case of overheal)
    int32 ActualHealing = FMath::Max(0, NewHealth - OldHealth);
    if (ActualHealing == 0 && FinalHealing > 0)
    {
        // Edge case: server says same HP but healing was non-zero (full HP heal)
        ActualHealing = FinalHealing;
    }

    // Show HP healing effect
    if (ActualHealing > 0)
    {
        ICombatable::Execute_ShowHealingEffect(TargetObject, ActualHealing, SkillResult.skillSlug);
    }

    // Show mana restore FCT if mana was gained (item-based mana potions / mana restore skills)
    if (SkillResult.finalTargetMana > 0)
    {
        int32 ActualManaHeal = FMath::Max(0, SkillResult.finalTargetMana - OldMana);
        if (ActualManaHeal == 0 && SkillResult.manaHealing > 0)
        {
            // Edge case: already at max mana, but server confirmed a restore happened
            ActualManaHeal = SkillResult.manaHealing;
        }
        if (ActualManaHeal > 0)
        {
            ICombatable::Execute_ShowManaRestoreEffect(TargetObject, ActualManaHeal);
        }
    }

    // Handle overheal if any
    if (FinalHealing > ActualHealing)
    {
        int32 OverhealAmount = FinalHealing - ActualHealing;
        UE_LOG(LogGameplay, Verbose, TEXT("HealingEffectHandler: Overheal of %d prevented for target %d"), 
            OverhealAmount, ICombatable::Execute_GetActorId(TargetObject));
    }

    LogHealingProcessing(SkillResult, TargetObject);
}

void UHealingEffectHandler::ProcessCriticalHeal(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogGameplay, Verbose, TEXT("HealingEffectHandler: Processing critical heal for %d healing"), SkillResult.healing);
    // Critical heal processing logic can be expanded here
}

void UHealingEffectHandler::ProcessHealingOverTime(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogGameplay, Verbose, TEXT("HealingEffectHandler: Processing HOT effect"));
    // HOT processing logic can be implemented here
}

void UHealingEffectHandler::LogHealingProcessing(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogGameplay, Verbose, TEXT("HealingEffectHandler: Processed %d healing to target %d (Current HP: %d/%d)"),
        SkillResult.healing, ICombatable::Execute_GetActorId(TargetObject), 
        ICombatable::Execute_GetCurrentHealth(TargetObject), ICombatable::Execute_GetMaxHealth(TargetObject));
}