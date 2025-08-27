#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/DataStructs.h"
#include "ISkillEffectHandler.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USkillEffectHandler : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for handling different types of skill effects
 */
class PROTOTYPING_API ISkillEffectHandler
{
    GENERATED_BODY()

public:
    /**
     * Check if this handler can process the given effect type
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Skill Effect Handler")
    bool CanHandle(ESkillEffectType EffectType) const;
    virtual bool CanHandle_Implementation(ESkillEffectType EffectType) const { return false; }

    /**
     * Process the skill result for the specific effect type
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Skill Effect Handler")
    void ProcessSkillResult(const FSkillResultData& SkillResult, const TScriptInterface<class ICombatable>& Target);
    virtual void ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<class ICombatable>& Target) {}

    /**
     * Get the priority for this handler (higher priority handlers are processed first)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Skill Effect Handler")
    int32 GetPriority() const;
    virtual int32 GetPriority_Implementation() const { return 0; }
};