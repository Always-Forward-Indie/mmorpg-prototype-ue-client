#include "Gameplay/Combat/DamageEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "UI/UIManager.h"
#include "MyGameInstance.h"

UDamageEffectHandler::UDamageEffectHandler()
{
}

bool UDamageEffectHandler::CanHandle_Implementation(ESkillEffectType EffectType) const
{
    return EffectType == ESkillEffectType::Damage;
}

void UDamageEffectHandler::ProcessSkillResult_Implementation(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        UE_LOG(LogTemp, Error, TEXT("DamageEffectHandler: Target interface is null or invalid"));
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // Применить здоровье/ману от сервера
    ICombatable::Execute_SetCurrentHealth(TargetObject, SkillResult.finalTargetHealth);
    ICombatable::Execute_SetCurrentMana(TargetObject, SkillResult.finalTargetMana);

    // Показать визуальные эффекты в зависимости от результата атаки
    if (SkillResult.isMissed)
    {
        ShowMissedEffect(SkillResult, Target);
    }
    else if (SkillResult.isBlocked)
    {
        ShowBlockedDamageEffect(SkillResult, Target);
    }
    else if (SkillResult.damage > 0)
    {
        ShowNormalDamageEffect(SkillResult, Target);
    }

    // Обработка критических ударов
    if (SkillResult.isCritical && !SkillResult.isMissed)
    {
        ProcessCriticalHit(SkillResult, TargetObject);
    }

    // Проверка смерти
    int32 CurrentHealth = ICombatable::Execute_GetCurrentHealth(TargetObject);
    if (CurrentHealth <= 0 && !ICombatable::Execute_IsDead(TargetObject))
    {
        ICombatable::Execute_SetDead(TargetObject, true);
        ICombatable::Execute_OnDeath(TargetObject);
        UE_LOG(LogTemp, Warning, TEXT("DamageEffectHandler: Target died from damage"));
    }

    LogDamageProcessing(SkillResult, TargetObject);
}

void UDamageEffectHandler::ShowNormalDamageEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // Показать урон через ICombatable интерфейс (для анимаций, звуков и т.д.)
    ICombatable::Execute_ShowDamageEffect(TargetObject, SkillResult.damage, SkillResult.isCritical, SkillResult.skillSchool);

    // Показать floating combat text
    ShowFloatingDamageText(SkillResult, Target);

    UE_LOG(LogTemp, Log, TEXT("DamageEffectHandler: Showed normal damage effect: %d damage"), SkillResult.damage);
}

void UDamageEffectHandler::ShowBlockedDamageEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // Показать эффекты блокировки через ICombatable
    ICombatable::Execute_ShowDamageEffect(TargetObject, SkillResult.damage, SkillResult.isCritical, SkillResult.skillSchool);

    // Показать "BLOCKED" текст через FloatingCombatTextManager
    if (UFloatingCombatTextManager* FCTManager = GetFCTManager(TargetObject))
    {
        FVector CombatPosition = ICombatable::Execute_GetCombatPosition(TargetObject);
        FCTManager->ShowBlockedText(CombatPosition);

        // Показать числа урона, если урон был нанесен несмотря на блок
        if (SkillResult.damage > 0)
        {
            ShowFloatingDamageText(SkillResult, Target);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("DamageEffectHandler: Showed blocked damage effect: %d damage"), SkillResult.damage);
}

void UDamageEffectHandler::ShowMissedEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // Показать "MISSED" текст
    if (UFloatingCombatTextManager* FCTManager = GetFCTManager(TargetObject))
    {
        FVector CombatPosition = ICombatable::Execute_GetCombatPosition(TargetObject);
        FCTManager->ShowMissText(CombatPosition);
    }

    UE_LOG(LogTemp, Log, TEXT("DamageEffectHandler: Showed missed effect"));
}

void UDamageEffectHandler::ShowFloatingDamageText(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        return;
    }

    UObject* TargetObject = Target.GetObject();

    if (UFloatingCombatTextManager* FCTManager = GetFCTManager(TargetObject))
    {
        FVector CombatPosition = ICombatable::Execute_GetCombatPosition(TargetObject);

        // Конвертировать ESkillSchool в EDamageType
        EDamageType DamageType = ConvertSkillSchoolToDamageType(SkillResult.skillSchool);

        FCTManager->ShowDamage(CombatPosition, SkillResult.damage, SkillResult.isCritical, DamageType);
    }
}

UFloatingCombatTextManager* UDamageEffectHandler::GetFCTManager(UObject* TargetObject)
{
    // Получить FCT Manager через GameInstance
    if (AActor* Actor = Cast<AActor>(TargetObject))
    {
        if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(Actor->GetGameInstance()))
        {
            if (UUIManager* GameUIManager = GameInstance->GetUIManager())
            {
                return GameUIManager->GetFCTManager();
            }
        }
    }
    return nullptr;
}

EDamageType UDamageEffectHandler::ConvertSkillSchoolToDamageType(ESkillSchool SkillSchool)
{
    switch (SkillSchool)
    {
    case ESkillSchool::Fire:
        return EDamageType::Fire;
    case ESkillSchool::Ice:
        return EDamageType::Ice;
    case ESkillSchool::Physical:
        return EDamageType::Physical;
    default:
        return EDamageType::Physical;
    }
}

void UDamageEffectHandler::ProcessCriticalHit(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Log, TEXT("DamageEffectHandler: Processing critical hit for %d damage"), SkillResult.damage);

    // Дополнительные эффекты для критических ударов
    // Например, специальные звуки, частицы и т.д.
}

void UDamageEffectHandler::ProcessDamageOverTime(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Log, TEXT("DamageEffectHandler: Processing DOT effect"));
    // DOT processing logic can be implemented here
}

void UDamageEffectHandler::LogDamageProcessing(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogTemp, Verbose, TEXT("DamageEffectHandler: Processed %d damage to target %d (Critical: %s, Blocked: %s, Missed: %s)"),
        SkillResult.damage, ICombatable::Execute_GetActorId(TargetObject),
        SkillResult.isCritical ? TEXT("Yes") : TEXT("No"),
        SkillResult.isBlocked ? TEXT("Yes") : TEXT("No"),
        SkillResult.isMissed ? TEXT("Yes") : TEXT("No"));
}