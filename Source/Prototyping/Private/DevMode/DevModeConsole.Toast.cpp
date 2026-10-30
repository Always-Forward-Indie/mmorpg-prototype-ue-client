#include "DevMode/DevModeConsoleCommands.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"

// ============================================================================
// devmode.toast — fire test toasts at the notification stack on demand.
// Usage: devmode.toast [type] [count]
//   no args        — 6-toast mix across severities (info/success/error)
//   devmode.toast item_received 3 — three receipts
// DevMode-only visual proof that the stack, severity tint and sounds work.
// ============================================================================
void UDevModeConsoleCommands::Cmd_Toast(const TArray<FString>& Args) const
{
    if (!GameInstance || !GameInstance->Player)
    {
        UE_LOG(LogGameplay, Warning, TEXT("Usage: devmode.toast [type] [count] (needs a spawned player)"));
        return;
    }
    UUIManager* UIMgr = GameInstance->Player->GetUIManager();
    UWorldNotificationManager* NotifMgr = UIMgr ? UIMgr->GetWorldNotificationManager() : nullptr;
    UNotificationStackWidget* Stack = NotifMgr ? NotifMgr->GetToastWidget() : nullptr;
    if (!Stack)
    {
        UE_LOG(LogGameplay, Error, TEXT("devmode.toast: notification stack not available"));
        return;
    }

    auto Fire = [Stack](const FString& Type, const TMap<FString, FString>& Fields)
    {
        Stack->EnqueueActionToast(Type, Fields);
    };

    if (Args.Num() == 0)
    {
        // Severity mix: info + success receipts + red failures.
        Fire(TEXT("zone_explored"), {});
        TMap<FString, FString> ItemFields;
        ItemFields.Add(TEXT("itemSlug"), TEXT("health_potion"));
        ItemFields.Add(TEXT("quantity"), TEXT("3"));
        Fire(TEXT("item_received"), ItemFields);
        TMap<FString, FString> GoldFields;
        GoldFields.Add(TEXT("amount"), TEXT("150"));
        Fire(TEXT("gold_received"), GoldFields);
        TMap<FString, FString> SkillFields;
        SkillFields.Add(TEXT("skillSlug"), TEXT("fireball"));
        Fire(TEXT("skill_learned"), SkillFields);
        TMap<FString, FString> QuestFields;
        QuestFields.Add(TEXT("clientQuestKey"), TEXT("demo_quest"));
        Fire(TEXT("quest_failed"), QuestFields);
        TMap<FString, FString> FailFields;
        FailFields.Add(TEXT("skillSlug"), TEXT("frost_nova"));
        FailFields.Add(TEXT("reason"), TEXT("insufficient_gold"));
        Fire(TEXT("learn_skill_failed"), FailFields);
        UE_LOG(LogGameplay, Warning, TEXT("devmode.toast: fired 6-toast severity mix"));
        return;
    }

    const FString Type = Args[0];
    int32 Count = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
    Count = FMath::Clamp(Count, 1, 8);
    for (int32 i = 0; i < Count; ++i)
    {
        TMap<FString, FString> Fields;
        Fields.Add(TEXT("quantity"), FString::FromInt(i + 1));
        Fire(Type, Fields);
    }
    UE_LOG(LogGameplay, Warning, TEXT("devmode.toast: fired %dx '%s'"), Count, *Type);
}
