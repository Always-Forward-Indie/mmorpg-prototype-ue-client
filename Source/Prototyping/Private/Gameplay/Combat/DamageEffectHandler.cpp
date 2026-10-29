#include "Gameplay/Combat/DamageEffectHandler.h"
#include "Prototyping.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "UI/UIManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"

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
        UE_LOG(LogGameplay, Error, TEXT("DamageEffectHandler: Target interface is null or invalid"));
        return;
    }

    UObject* TargetObject = Target.GetObject();

    UE_LOG(LogGameplay, Warning, TEXT("DamageEffectHandler: Processing result for target %s - Damage=%d HP=%d?%d Died=%s"), 
        *GetNameSafe(TargetObject), SkillResult.damage, 
        ICombatable::Execute_GetCurrentHealth(TargetObject), SkillResult.finalTargetHealth,
        SkillResult.targetDied ? TEXT("YES") : TEXT("NO"));

    // Skip HP/MP application when the skill failed with no effect (e.g. PvP blocked).
    // The server leaves finalTargetHealth at 0 in these cases, which would kill the
    // target client-side even though no damage was actually dealt.
    const bool bFailedWithNoEffect = !SkillResult.success && SkillResult.damage == 0 && SkillResult.healing == 0;
    if (bFailedWithNoEffect)
    {
        UE_LOG(LogGameplay, Warning, TEXT("DamageEffectHandler: Skipping HP/MP update — skill '%s' failed with no effect (reason: %s)"),
            *SkillResult.skillName, *SkillResult.errorReason);
    }
    // Apply HP/Mana changes for hits and blocks — skip only for misses
    // (server doesn't send finalTargetHealth for error/miss results).
    // Single-authority rule for the LOCAL player: vitals are owned exclusively
    // by stats_update (the server pushes one per damage/heal event, including
    // the new mob-AI victim push). A deferred combatResult carries an OLD
    // finalTargetHealth and would yank the bar backward (log-proven 46->61,
    // 15->30 jumps under fox packs). Damage numbers, death detection and
    // mob/remote handling below are unaffected. Remote players keep
    // combat-driven HP — they get no per-hit stats_updates. Mobs are
    // single-channel on mobHealthUpdate (server pushes one per landed hit,
    // plus regen/effectTick/spawn) — see bSkipVitalsForMob below.
    else if (!SkillResult.isMissed)
    {
        bool bSkipVitalsForLocalPlayer = false;
        bool bSkipVitalsForMob = (Cast<ABasicMOB>(TargetObject) != nullptr);
        if (ABasicPlayer* TargetPlayer = Cast<ABasicPlayer>(TargetObject))
        {
            bSkipVitalsForLocalPlayer = !TargetPlayer->GetIsOtherClient();
        }
        if (bSkipVitalsForLocalPlayer)
        {
            UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: skipping combat HP set for local player %s (stats_update owns vitals; result HP=%d)"),
                *GetNameSafe(TargetObject), SkillResult.finalTargetHealth);
        }
        else if (bSkipVitalsForMob)
        {
            UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: skipping combat HP set for mob %s (mobHealthUpdate owns vitals; result HP=%d)"),
                *GetNameSafe(TargetObject), SkillResult.finalTargetHealth);
        }
        else
        {
            ICombatable::Execute_SetCurrentHealth(TargetObject, SkillResult.finalTargetHealth);
            if (SkillResult.finalTargetMana > 0)
            {
                ICombatable::Execute_SetCurrentMana(TargetObject, SkillResult.finalTargetMana);
            }
        }
    }

    // �������� ���������� ������� � ����������� �� ���������� �����
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

    // Trigger hit-react animation on mob targets (only for actual damage, not misses)
    if (!SkillResult.isMissed && SkillResult.damage > 0)
    {
        if (ABasicMOB* Mob = Cast<ABasicMOB>(TargetObject))
        {
            if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(Mob->GetMesh()->GetAnimInstance()))
            {
                AnimInst->NotifyHit();
            }
        }
    }

    // ��������� ����������� ������
    if (SkillResult.isCritical && !SkillResult.isMissed)
    {
        ProcessCriticalHit(SkillResult, TargetObject);
    }

    // Death check � use server-authoritative targetDied flag as primary source,
    // with a fallback check on HP for safety.
    if (!ICombatable::Execute_IsDead(TargetObject))
    {
        if (SkillResult.targetDied || ICombatable::Execute_GetCurrentHealth(TargetObject) <= 0)
        {
            // Grace window for players: ignore death within 5s of respawn to prevent
            // a stale combatResult from re-killing a just-revived character.
            if (ABasicPlayer* Player = Cast<ABasicPlayer>(TargetObject))
            {
                if (SkillResult.targetDied)
                {
                    const float TimeSinceRespawn = Player->GetLastRespawnWorldTime() > 0.f
                        ? (Player->GetWorld() ? Player->GetWorld()->GetTimeSeconds() - Player->GetLastRespawnWorldTime() : 999.f)
                        : 999.f;
                    if (TimeSinceRespawn < 5.0f)
                    {
                        UE_LOG(LogGameplay, Warning, TEXT("DamageEffectHandler: Ignoring targetDied for player %s - within respawn grace window (%.1fs)"),
                            *GetNameSafe(TargetObject), TimeSinceRespawn);
                        return;
                    }
                }
            }

            UE_LOG(LogGameplay, Warning, TEXT("DamageEffectHandler: Target %s is DEAD - calling SetDead(true)"), 
                *GetNameSafe(TargetObject));
            // SetDead(true) already calls OnDeath internally � do NOT call OnDeath separately.
            ICombatable::Execute_SetDead(TargetObject, true);
        }
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

    // �������� ���� ����� ICombatable ��������� (��� ��������, ������ � �.�.)
    ICombatable::Execute_ShowDamageEffect(TargetObject, SkillResult.damage, SkillResult.isCritical, SkillResult.skillSchool, false, false, SkillResult.skillSlug);

    // �������� floating combat text
    ShowFloatingDamageText(SkillResult, Target);

    UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: Showed normal damage effect: %d damage"), SkillResult.damage);
}

void UDamageEffectHandler::ShowBlockedDamageEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // �������� ������� ���������� ����� ICombatable
    ICombatable::Execute_ShowDamageEffect(TargetObject, SkillResult.damage, SkillResult.isCritical, SkillResult.skillSchool, false, true, SkillResult.skillSlug);

    // �������� "BLOCKED" ����� ����� FloatingCombatTextManager
    if (UFloatingCombatTextManager* FCTManager = GetFCTManager(TargetObject))
    {
        FVector CombatPosition = ICombatable::Execute_GetCombatPosition(TargetObject);
        FCTManager->ShowBlockedText(CombatPosition);

        // �������� ����� �����, ���� ���� ��� ������� �������� �� ����
        if (SkillResult.damage > 0)
        {
            ShowFloatingDamageText(SkillResult, Target);
        }
    }

    UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: Showed blocked damage effect: %d damage"), SkillResult.damage);
}

void UDamageEffectHandler::ShowMissedEffect(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        return;
    }

    UObject* TargetObject = Target.GetObject();

    // �������� "MISSED" �����
    if (UFloatingCombatTextManager* FCTManager = GetFCTManager(TargetObject))
    {
        FVector CombatPosition = ICombatable::Execute_GetCombatPosition(TargetObject);
        FCTManager->ShowMissText(CombatPosition);
    }

    UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: Showed missed effect"));
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

        // �������������� ESkillSchool � EDamageType
        EDamageType DamageType = ConvertSkillSchoolToDamageType(SkillResult.skillSchool);

        // Detect whether the target is the local player so damage-on-self numbers
        // render with a larger, more prominent style.
        bool bOnLocalPlayer = false;
        if (ABasicPlayer* Player = Cast<ABasicPlayer>(TargetObject))
        {
            bOnLocalPlayer = !Player->GetIsOtherClient();
        }

        FCTManager->ShowDamage(CombatPosition, SkillResult.damage, SkillResult.isCritical, DamageType, bOnLocalPlayer);
    }
}

UFloatingCombatTextManager* UDamageEffectHandler::GetFCTManager(UObject* TargetObject)
{
    if (!TargetObject || !TargetObject->IsA(AActor::StaticClass()))
    {
        UE_LOG(LogGameplay, Error, TEXT("GetFCTManager: TargetObject is not an AActor"));
        return nullptr;
    }

    AActor* Actor = static_cast<AActor*>(TargetObject); // ����� � ��� RTTI
    UWorld* World = Actor->GetWorld();
    if (!World)
    {
        UE_LOG(LogGameplay, Error, TEXT("GetFCTManager: World is null"));
        return nullptr;
    }

        APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(World, 0);
        if (!LocalPawn)
        {
            UE_LOG(LogGameplay, Error, TEXT("GetFCTManager: No local player pawn"));
            return nullptr;
        }

        if (UUIManager* UIM = LocalPawn->FindComponentByClass<UUIManager>())
        {
            if (UFloatingCombatTextManager* FCT = UIM->GetFCTManager())
            {
                return FCT;
            }
            UE_LOG(LogGameplay, Error, TEXT("GetFCTManager: Local player's UIManager has no FCT"));
        }
        else
        {
            UE_LOG(LogGameplay, Error, TEXT("GetFCTManager: Local player has no UUIManager component"));
        }


    UE_LOG(LogGameplay, Error, TEXT("GetFCTManager: Failed to resolve FCT (Target=%s)"), *GetNameSafe(Actor));
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
    UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: Processing critical hit for %d damage"), SkillResult.damage);

    // �������������� ������� ��� ����������� ������
    // ��������, ����������� �����, ������� � �.�.
}

void UDamageEffectHandler::ProcessDamageOverTime(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: Processing DOT effect"));
    // DOT processing logic can be implemented here
}

void UDamageEffectHandler::LogDamageProcessing(const FSkillResultData& SkillResult, UObject* TargetObject)
{
    UE_LOG(LogGameplay, Verbose, TEXT("DamageEffectHandler: Processed %d damage to target %d (Critical: %s, Blocked: %s, Missed: %s)"),
        SkillResult.damage, ICombatable::Execute_GetActorId(TargetObject),
        SkillResult.isCritical ? TEXT("Yes") : TEXT("No"),
        SkillResult.isBlocked ? TEXT("Yes") : TEXT("No"),
        SkillResult.isMissed ? TEXT("Yes") : TEXT("No"));
}