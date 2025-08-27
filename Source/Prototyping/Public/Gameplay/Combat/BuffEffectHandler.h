#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "BuffEffectHandler.generated.h"

// Forward declaration
class ICombatable;

/**
 * Handler for buff and debuff effects
 * Processes temporary status effects on targets
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UBuffEffectHandler : public UObject, public ISkillEffectHandler
{
    GENERATED_BODY()

public:
    UBuffEffectHandler();

    // ISkillEffectHandler interface implementation
    virtual bool CanHandle_Implementation(ESkillEffectType EffectType) const override;
    virtual void ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target) override;
    virtual int32 GetPriority_Implementation() const override { return 80; } // Medium priority for buffs

protected:
    // Helper methods for different buff/debuff types
    void ProcessBuffApplication(const FSkillResultData& SkillResult, UObject* TargetObject);
    void ProcessDebuffApplication(const FSkillResultData& SkillResult, UObject* TargetObject);
    
    // Effect management methods
    void ApplyEffect(UObject* TargetObject, const FAppliedEffectData& Effect, bool bIsBuff);
    void RemoveConflictingEffects(UObject* TargetObject, const FAppliedEffectData& NewEffect);
    bool ShouldStackEffect(UObject* TargetObject, const FAppliedEffectData& Effect);

private:
    // Log buff processing for debugging
    void LogBuffProcessing(const FSkillResultData& SkillResult, UObject* TargetObject);
};