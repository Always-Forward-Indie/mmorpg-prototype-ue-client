#include "UI/NotificationToastWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "TimerManager.h"

void UNotificationToastWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
    bIsShowing = false;
}

void UNotificationToastWidget::ShowNotification(const FWorldNotificationStruct& Notification)
{
    if (bIsShowing)
    {
        // Queue the notification; drop oldest if queue is full
        if (PendingQueue.Num() >= MaxQueueSize)
        {
            PendingQueue.RemoveAt(0);
        }
        PendingQueue.Add(Notification);
        return;
    }

    DisplayNotificationImmediate(Notification);
}

void UNotificationToastWidget::EnqueueActionToast(const FString& NotificationType, const TMap<FString, FString>& DataFields)
{
    FWorldNotificationStruct Notif;
    Notif.notificationType = NotificationType;
    Notif.channel          = TEXT("toast");
    Notif.priority         = TEXT("medium");
    Notif.dataFields       = DataFields;
    ShowNotification(Notif);
}

void UNotificationToastWidget::DisplayNotificationImmediate(const FWorldNotificationStruct& Notification)
{
    bIsShowing = true;

    // Clear any previous auto-hide timer
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(AutoHideTimer);

    FText Title, Body;
    BuildDisplayText(Notification, Title, Body);

    if (Toast_Title_Text)
        Toast_Title_Text->SetText(Title);
    if (Toast_Body_Text)
        Toast_Body_Text->SetText(Body.IsEmpty() ? Title : Body);

    // Optional icon from locale
    if (Toast_Icon)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                FNotificationLocaleDefinition Def;
                if (Loc->GetNotificationLocaleDefinition(Notification.notificationType, Def) && Def.Icon.IsValid())
                {
                    if (UTexture2D* Tex = Def.Icon.LoadSynchronous())
                        Toast_Icon->SetBrushFromTexture(Tex);
                }
                else
                {
                    Toast_Icon->SetBrushFromTexture(nullptr);
                }
            }
        }
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    PlayShowAnimation();

    // Schedule auto-hide
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(AutoHideTimer, FTimerDelegate::CreateWeakLambda(this,
            [this]()
            {
                PlayHideAnimation();
                // After hide animation starts, schedule showing next queued toast
                // Give a small delay for the hide animation to play
                if (UWorld* W = GetWorld())
                {
                    FTimerHandle NextTimer;
                    W->GetTimerManager().SetTimer(NextTimer, FTimerDelegate::CreateWeakLambda(this,
                        [this]()
                        {
                            ShowNextInQueue();
                        }), 0.3f, false);
                }
            }), DisplayDuration, false);
    }
}

void UNotificationToastWidget::ShowNextInQueue()
{
    bIsShowing = false;

    if (PendingQueue.Num() > 0)
    {
        FWorldNotificationStruct Next = PendingQueue[0];
        PendingQueue.RemoveAt(0);
        DisplayNotificationImmediate(Next);
    }
}

void UNotificationToastWidget::BuildDisplayText(const FWorldNotificationStruct& Notification,
                                                 FText& OutTitle, FText& OutBody) const
{
    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    const FString& Type = Notification.notificationType;

    // Try to get the title from the notification locale table first.
    // TextTemplate / Title are filled by designers in DT_NotificationLocale.
    FNotificationLocaleDefinition NotifDef;
    const bool bHasLocale = Loc && Loc->GetNotificationLocaleDefinition(Type, NotifDef);

    if (Type == TEXT("bestiary_tier_unlocked"))
    {
        const FString MobSlug = Notification.dataFields.FindRef(TEXT("mobSlug"));
        const FString CatSlug = Notification.dataFields.FindRef(TEXT("categorySlug"));
        FText MobName = Loc ? Loc->GetMobDisplayName(MobSlug) : FText::FromString(MobSlug);
        FText CatName = Loc ? Loc->GetBestiaryCategoryName(CatSlug) : FText::FromString(CatSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Bestiary unlocked"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s \u2014 %s"),
            *MobName.ToString(), *CatName.ToString()));
    }
    else if (Type == TEXT("zone_explored"))
    {
        const FString ZoneSlug = Notification.dataFields.FindRef(TEXT("zoneSlug"));
        const FString XpStr    = Notification.dataFields.FindRef(TEXT("xpGained"));
        FText ZoneName = Loc ? Loc->GetZoneDisplayName(ZoneSlug) : FText::FromString(ZoneSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Zone discovered"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s (+%s XP)"),
            *ZoneName.ToString(), *XpStr));
    }
    else if (Type == TEXT("mastery_tier_up"))
    {
        const FString MasterySlug = Notification.dataFields.FindRef(TEXT("masterySlug"));
        const FString TierStr     = Notification.dataFields.FindRef(TEXT("tier"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Mastery increased"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s \u2014 tier %s"), *MasterySlug, *TierStr));
    }
    else if (Type == TEXT("champion_spawned"))
    {
        const FString MobSlug = Notification.dataFields.FindRef(TEXT("mobSlug"));
        FText MobName = Loc ? Loc->GetMobDisplayName(MobSlug) : FText::FromString(MobSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Champion appeared!"));
        OutBody  = MobName;
    }
    else if (Type == TEXT("champion_killed"))
    {
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Champion defeated!"));
        OutBody  = FText::GetEmpty();
    }
    // ====================================================================
    // Dialogue action notification types
    // ====================================================================
    else if (Type == TEXT("quest_offered"))
    {
        const FString QuestKey = Notification.dataFields.FindRef(TEXT("clientQuestKey"));
        FText QuestName = Loc ? Loc->GetNotificationTextTemplate(QuestKey) : FText::GetEmpty();
        if (QuestName.IsEmpty()) QuestName = FText::FromString(QuestKey);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("New Quest"));
        OutBody  = QuestName;
    }
    else if (Type == TEXT("quest_turned_in"))
    {
        const FString QuestKey = Notification.dataFields.FindRef(TEXT("clientQuestKey"));
        FText QuestName = Loc ? Loc->GetNotificationTextTemplate(QuestKey) : FText::GetEmpty();
        if (QuestName.IsEmpty()) QuestName = FText::FromString(QuestKey);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Quest Complete"));
        OutBody  = QuestName;
    }
    else if (Type == TEXT("quest_failed"))
    {
        const FString QuestKey = Notification.dataFields.FindRef(TEXT("clientQuestKey"));
        FText QuestName = Loc ? Loc->GetNotificationTextTemplate(QuestKey) : FText::GetEmpty();
        if (QuestName.IsEmpty()) QuestName = FText::FromString(QuestKey);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Quest Failed"));
        OutBody  = QuestName;
    }
    else if (Type == TEXT("item_received"))
    {
        const FString ItemId   = Notification.dataFields.FindRef(TEXT("itemId"));
        const FString Quantity = Notification.dataFields.FindRef(TEXT("quantity"));
        const FString ItemSlug = Notification.dataFields.FindRef(TEXT("itemSlug"));
        FText ItemName = FText::GetEmpty();
        if (Loc && !ItemSlug.IsEmpty())
            ItemName = Loc->GetItemDisplayName(ItemSlug);
        if (ItemName.IsEmpty())
            ItemName = FText::FromString(ItemSlug.IsEmpty() ? FString::Printf(TEXT("Item #%s"), *ItemId) : ItemSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Item Received"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s x%s"),
            *ItemName.ToString(), *Quantity));
    }
    else if (Type == TEXT("exp_received"))
    {
        const FString Amount = Notification.dataFields.FindRef(TEXT("amount"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Experience"));
        OutBody  = FText::FromString(FString::Printf(TEXT("+%s XP"), *Amount));
    }
    else if (Type == TEXT("gold_received"))
    {
        const FString Amount = Notification.dataFields.FindRef(TEXT("amount"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Gold"));
        OutBody  = FText::FromString(FString::Printf(TEXT("+%s Gold"), *Amount));
    }
    else if (Type == TEXT("skill_learned"))
    {
        const FString SkillSlug = Notification.dataFields.FindRef(TEXT("skillSlug"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Skill Learned"));
        OutBody  = FText::FromString(SkillSlug);
    }
    else if (Type == TEXT("learn_skill_failed"))
    {
        const FString Reason    = Notification.dataFields.FindRef(TEXT("reason"));
        const FString SkillSlug = Notification.dataFields.FindRef(TEXT("skillSlug"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Cannot Learn Skill"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s (%s)"), *SkillSlug, *Reason));
    }
    else if (Type == TEXT("reputationChanged"))
    {
        const FString Faction = Notification.dataFields.FindRef(TEXT("faction"));
        const FString Delta   = Notification.dataFields.FindRef(TEXT("delta"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Reputation"));
        // Show +/- prefix based on sign
        FString Prefix = TEXT("+");
        if (Delta.StartsWith(TEXT("-"))) Prefix = TEXT("");
        OutBody  = FText::FromString(FString::Printf(TEXT("%s: %s%s"), *Faction, *Prefix, *Delta));
    }
    else
    {
        // Generic fallback: use textTemplate from locale, or the raw type slug
        OutTitle = bHasLocale && !NotifDef.textTemplate.IsEmpty()
            ? NotifDef.textTemplate
            : (Loc ? Loc->GetNotificationTextTemplate(Type) : FText::FromString(Type));
        OutBody = FText::GetEmpty();
    }
}
