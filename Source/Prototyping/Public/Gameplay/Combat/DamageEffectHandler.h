#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "Data/DataStructs.h"
#include "DamageEffectHandler.generated.h"

// Forward declarations
class ICombatable;
class UFloatingCombatTextManager;

/**
 * Handler for damage-type skills
 * Processes physical, magical, and elemental damage with visual effects
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UDamageEffectHandler : public UObject, public ISkillEffectHandler
{
    GENERATED_BODY()

public:
    UDamageEffectHandler();

    // ISkillEffectHandler interface implementation
    virtual bool CanHandle_Implementation(ESkillEffectType EffectType) const override;
    virtual void ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target) override;
    virtual int32 GetPriority_Implementation() const override { return 100; } // High priority for damage

protected:
    // Visual effect methods
    UFUNCTION(BlueprintCallable, Category = "Damage Effects")
    void ShowNormalDamageEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target);

    UFUNCTION(BlueprintCallable, Category = "Damage Effects")
    void ShowBlockedDamageEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target);

    UFUNCTION(BlueprintCallable, Category = "Damage Effects")
    void ShowMissedEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target);

    UFUNCTION(BlueprintCallable, Category = "Damage Effects")
    void ShowFloatingDamageText(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target);

    // Helper methods for damage processing
    void ProcessCriticalHit(const FSkillResultData& SkillResult, UObject* TargetObject);
    void ProcessDamageOverTime(const FSkillResultData& SkillResult, UObject* TargetObject);

private:
    // Utility methods
    UFloatingCombatTextManager* GetFCTManager(UObject* TargetObject);
    EDamageType ConvertSkillSchoolToDamageType(ESkillSchool SkillSchool);
    void LogDamageProcessing(const FSkillResultData& SkillResult, UObject* TargetObject);
};