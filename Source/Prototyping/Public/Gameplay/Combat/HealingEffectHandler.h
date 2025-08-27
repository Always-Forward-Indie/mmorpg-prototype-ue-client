#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "HealingEffectHandler.generated.h"

// Forward declaration
class ICombatable;

/**
 * Handler for healing-type skills
 * Processes health and mana restoration
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UHealingEffectHandler : public UObject, public ISkillEffectHandler
{
    GENERATED_BODY()

public:
    UHealingEffectHandler();

    // ISkillEffectHandler interface implementation
    virtual bool CanHandle_Implementation(ESkillEffectType EffectType) const override;
    virtual void ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target) override;
    virtual int32 GetPriority_Implementation() const override { return 90; } // High priority for healing

protected:
    // Helper methods for different healing types
    void ProcessCriticalHeal(const FSkillResultData& SkillResult, UObject* TargetObject);
    
    // Handle healing over time effects
    void ProcessHealingOverTime(const FSkillResultData& SkillResult, UObject* TargetObject);

private:
    // Log healing processing for debugging
    void LogHealingProcessing(const FSkillResultData& SkillResult, UObject* TargetObject);
};