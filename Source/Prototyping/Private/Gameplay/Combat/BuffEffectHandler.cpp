#include "Gameplay/Combat/BuffEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"

UBuffEffectHandler::UBuffEffectHandler()
{
}

bool UBuffEffectHandler::CanHandle_Implementation(ESkillEffectType EffectType) const
{
    return EffectType == ESkillEffectType::Buff || EffectType == ESkillEffectType::Debuff;
}

void UBuffEffectHandler::ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        UE_LOG(LogTemp, Error, TEXT("BuffEffectHandler: Target interface is null or invalid"));
        return;
    }

    UObject* TargetObject = Target.GetObject();

    bool bIsBuff = (SkillResult.skillEffectType == ESkillEffectType::Buff);
    FString EffectTypeName = bIsBuff ? TEXT("Buff") : TEXT("Debuff");

    UE_LOG(LogTemp, Log, TEXT("BuffEffectHandler: Processing %s effects to target %d"), 
        *EffectTypeName, ICombatable::Execute_GetActorId(TargetObject));

    // Process each applied effect
    for (const FAppliedEffectData& Effect : SkillResult.appliedEffects)
    {
        UE_LOG(LogTemp, Log, TEXT("BuffEffectHandler: Applying %s: %s (Value: %d, Duration: %.1f)"), 
            *EffectTypeName, *Effect.effectName, Effect.value, Effect.duration);

        // Apply the effect directly
        ApplyEffect(TargetObject, Effect, bIsBuff);
    }

    LogBuffProcessing(SkillResult, TargetObject);
}

void UBuffEffectHandler::ProcessBuffApplication(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Log, TEXT("BuffEffectHandler: Processing buff application"));
    // Buff-specific processing logic can be implemented here
}

void UBuffEffectHandler::ProcessDebuffApplication(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Log, TEXT("BuffEffectHandler: Processing debuff application"));
    // Debuff-specific processing logic can be implemented here
}

void UBuffEffectHandler::ApplyEffect(UObject* TargetObject, const FAppliedEffectData& Effect, bool bIsBuff)
{
    if (!TargetObject || !IsValid(TargetObject))
    {
        UE_LOG(LogTemp, Error, TEXT("BuffEffectHandler::ApplyEffect: Target is null or invalid"));
        return;
    }

    int32 TargetId = ICombatable::Execute_GetActorId(TargetObject);
    
    UE_LOG(LogTemp, Log, TEXT("BuffEffectHandler: Applied %s effect '%s' to target %d"), 
        bIsBuff ? TEXT("Buff") : TEXT("Debuff"), *Effect.effectName, TargetId);
    
    // Show the buff/debuff effect on the target
    ICombatable::Execute_ShowBuffEffect(TargetObject, Effect);
    
    // TODO: Implement actual stat modifications here
    // This would involve modifying target's stats based on effect type and value
    // For example:
    // - Damage boost: increase attack power
    // - Speed boost: increase movement speed
    // - Poison: deal damage over time
}

void UBuffEffectHandler::RemoveConflictingEffects(UObject* TargetObject, const FAppliedEffectData& NewEffect)
{
    if (!TargetObject || !IsValid(TargetObject))
    {
        UE_LOG(LogTemp, Error, TEXT("BuffEffectHandler::RemoveConflictingEffects: Target is null or invalid"));
        return;
    }

    int32 TargetId = ICombatable::Execute_GetActorId(TargetObject);
    
    UE_LOG(LogTemp, Log, TEXT("BuffEffectHandler: Checking for conflicting effects on target %d"), TargetId);
    
    // TODO: Implement conflict resolution logic
    // This would require tracking active effects somehow
    // For now, just log that we checked
}

bool UBuffEffectHandler::ShouldStackEffect(UObject* TargetObject, const FAppliedEffectData& Effect)
{
    if (!TargetObject || !IsValid(TargetObject))
    {
        return false;
    }

    // For now, assume effects don't stack by default
    // TODO: Implement proper stacking rules
    return false;
}

void UBuffEffectHandler::LogBuffProcessing(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Verbose, TEXT("BuffEffectHandler: Processed %d buff/debuff effects on target %d"),
        SkillResult.appliedEffects.Num(), ICombatable::Execute_GetActorId(TargetObject));
}