#include "Gameplay/Combat/HealingEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"

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
        UE_LOG(LogTemp, Error, TEXT("HealingEffectHandler: Target interface is null or invalid"));
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

    // Apply healing using Execute_ methods
    int32 CurrentHealth = ICombatable::Execute_GetCurrentHealth(TargetObject);
    int32 MaxHealth = ICombatable::Execute_GetMaxHealth(TargetObject);
    int32 NewHealth = FMath::Min(MaxHealth, CurrentHealth + FinalHealing);
    
    // Calculate actual healing done (in case of overheal)
    int32 ActualHealing = NewHealth - CurrentHealth;
    
    ICombatable::Execute_SetCurrentHealth(TargetObject, NewHealth);

    // Show healing effect
    ICombatable::Execute_ShowHealingEffect(TargetObject, ActualHealing);

    // Handle overheal if any
    if (FinalHealing > ActualHealing)
    {
        int32 OverhealAmount = FinalHealing - ActualHealing;
        UE_LOG(LogTemp, Log, TEXT("HealingEffectHandler: Overheal of %d prevented for target %d"), 
            OverhealAmount, ICombatable::Execute_GetActorId(TargetObject));
    }

    LogHealingProcessing(SkillResult, TargetObject);
}

void UHealingEffectHandler::ProcessCriticalHeal(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Log, TEXT("HealingEffectHandler: Processing critical heal for %d healing"), SkillResult.healing);
    // Critical heal processing logic can be expanded here
}

void UHealingEffectHandler::ProcessHealingOverTime(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Log, TEXT("HealingEffectHandler: Processing HOT effect"));
    // HOT processing logic can be implemented here
}

void UHealingEffectHandler::LogHealingProcessing(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Verbose, TEXT("HealingEffectHandler: Processed %d healing to target %d (Current HP: %d/%d)"),
        SkillResult.healing, ICombatable::Execute_GetActorId(TargetObject), 
        ICombatable::Execute_GetCurrentHealth(TargetObject), ICombatable::Execute_GetMaxHealth(TargetObject));
}