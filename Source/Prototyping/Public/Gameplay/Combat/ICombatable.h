#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/DataStructs.h"
#include "ICombatable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCombatable : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for objects that can participate in combat
 * Uses proper UE5 UINTERFACE system for type safety
 */
class PROTOTYPING_API ICombatable
{
    GENERATED_BODY()

public:
    // Actor identification
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable")
    int32 GetActorId() const;
    virtual int32 GetActorId_Implementation() const { return 0; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable")
    ECasterType GetActorType() const;
    virtual ECasterType GetActorType_Implementation() const { return ECasterType::None; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable")
    FString GetActorTypeString() const;
    virtual FString GetActorTypeString_Implementation() const { return TEXT("None"); }

    // Health/Mana management
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Health")
    int32 GetCurrentHealth() const;
    virtual int32 GetCurrentHealth_Implementation() const { return 0; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Health")
    int32 GetMaxHealth() const;
    virtual int32 GetMaxHealth_Implementation() const { return 0; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Mana")
    int32 GetCurrentMana() const;
    virtual int32 GetCurrentMana_Implementation() const { return 0; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Mana")
    int32 GetMaxMana() const;
    virtual int32 GetMaxMana_Implementation() const { return 0; }
    
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Health")
    void SetCurrentHealth(int32 NewHealth);
    virtual void SetCurrentHealth_Implementation(int32 NewHealth) {}

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Mana")
    void SetCurrentMana(int32 NewMana);
    virtual void SetCurrentMana_Implementation(int32 NewMana) {}

    // Combat state
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|State")
    bool IsDead() const;
    virtual bool IsDead_Implementation() const { return false; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|State")
    void SetDead(bool bNewDead);
    virtual void SetDead_Implementation(bool bNewDead) {}

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|State")
    void OnDeath();
    virtual void OnDeath_Implementation() {}

    // Position for damage text/effects
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Position")
    FVector GetCombatPosition() const;
    virtual FVector GetCombatPosition_Implementation() const { return FVector::ZeroVector; }
    
    // Target system
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Targeting")
    void SetTarget(int32 TargetId, ECasterType TargetType);
    virtual void SetTarget_Implementation(int32 TargetId, ECasterType TargetType) {}

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Targeting")
    void ClearTarget();
    virtual void ClearTarget_Implementation() {}
    
    // Animation/visual feedback
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Animation")
    void PlaySkillAnimation(const FString& AnimationName, float Duration = 0.0f);
    virtual void PlaySkillAnimation_Implementation(const FString& AnimationName, float Duration = 0.0f) {}

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Effects")
    void ShowDamageEffect(int32 Damage, bool bIsCritical, ESkillSchool School);
    virtual void ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School) {}

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Effects")
    void ShowHealingEffect(int32 Healing);
    virtual void ShowHealingEffect_Implementation(int32 Healing) {}

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combatable|Effects")
    void ShowBuffEffect(const FAppliedEffectData& Effect);
    virtual void ShowBuffEffect_Implementation(const FAppliedEffectData& Effect) {}
};