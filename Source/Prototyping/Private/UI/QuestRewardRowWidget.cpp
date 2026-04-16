#include "UI/QuestRewardRowWidget.h"
#include "Services/LocalizationSubsystem.h"
#include "Gameplay/Items/ItemManager.h"
#include "MyGameInstance.h"
#include "Engine/AssetManager.h"

void UQuestRewardRowWidget::SetupFromQuestReward(const FQuestRewardData& Reward)
{
    if (Reward.rewardType == TEXT("exp"))
    {
        Populate(TEXT("exp"), Reward.amount, /*bIsItem=*/false, /*bIsHidden=*/false);
    }
    else if (Reward.rewardType == TEXT("gold"))
    {
        Populate(TEXT("gold"), Reward.amount, false, false);
    }
    else if (Reward.rewardType == TEXT("item"))
    {
        Populate(Reward.itemSlug, Reward.quantity, /*bIsItem=*/true, Reward.isHidden);
    }
}

void UQuestRewardRowWidget::SetupFromGiftItem(const FGiftPreviewItem& Gift)
{
    if (Gift.giftType == TEXT("exp"))
    {
        Populate(TEXT("exp"), Gift.amount, false, false);
    }
    else if (Gift.giftType == TEXT("gold"))
    {
        Populate(TEXT("gold"), Gift.amount, false, false);
    }
    else if (Gift.giftType == TEXT("item"))
    {
        Populate(Gift.itemSlug, Gift.quantity, /*bIsItem=*/true, /*bIsHidden=*/false);
    }
}

void UQuestRewardRowWidget::Populate(const FString& TypeOrSlug, int32 Amount, bool bIsItem, bool bIsHidden)
{
    ULocalizationSubsystem* LocSys = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        LocSys = GI->GetSubsystem<ULocalizationSubsystem>();
    }

    // ── Name ────────────────────────────────────────────────────────────────
    if (Reward_Name_Text)
    {
        FText NameText;
        if (bIsItem)
        {
            if (bIsHidden)
            {
                NameText = FText::FromString(TEXT("???"));
            }
            else if (LocSys && !TypeOrSlug.IsEmpty())
            {
                NameText = LocSys->GetItemDisplayName(TypeOrSlug);
            }
            else
            {
                NameText = FText::FromString(TypeOrSlug.IsEmpty() ? TEXT("???") : TypeOrSlug);
            }
        }
        else if (TypeOrSlug == TEXT("exp"))
        {
            NameText = FText::FromString(TEXT("Experience"));
        }
        else if (TypeOrSlug == TEXT("gold"))
        {
            NameText = FText::FromString(TEXT("Gold"));
        }
        Reward_Name_Text->SetText(NameText);
    }

    // ── Amount ───────────────────────────────────────────────────────────────
    if (Reward_Amount_Text)
    {
        const FString AmountStr = bIsItem
            ? FString::Printf(TEXT("\u00D7%d"), Amount)   // ×N for items
            : FString::Printf(TEXT("%d"), Amount);
        Reward_Amount_Text->SetText(FText::FromString(AmountStr));
    }

    // ── Icon ─────────────────────────────────────────────────────────────────
    if (Reward_Icon)
    {
        if (bIsItem && !bIsHidden && !TypeOrSlug.IsEmpty())
        {
            LoadItemIcon(TypeOrSlug);
        }
        else
        {
            Reward_Icon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UQuestRewardRowWidget::LoadItemIcon(const FString& ItemSlug)
{
    if (!Reward_Icon) return;

    UMyGameInstance* MyGI = Cast<UMyGameInstance>(GetGameInstance());
    if (!MyGI)
    {
        Reward_Icon->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    UItemManager* ItemMgr = MyGI->GetItemManager();
    if (!ItemMgr)
    {
        Reward_Icon->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FItemVisualData VisualData = ItemMgr->GetItemVisualDataBySlug(ItemSlug);
    if (VisualData.Icon.IsNull())
    {
        Reward_Icon->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (UTexture2D* AlreadyLoaded = VisualData.Icon.Get())
    {
        Reward_Icon->SetBrushFromTexture(AlreadyLoaded, false);
        Reward_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
        return;
    }

    TWeakObjectPtr<UQuestRewardRowWidget> WeakThis = this;
    TSoftObjectPtr<UTexture2D> IconSoftPtr = VisualData.Icon;
    AsyncLoad(IconSoftPtr.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, IconSoftPtr]()
        {
            if (WeakThis.IsValid())
            {
                if (UTexture2D* Tex = IconSoftPtr.Get())
                {
                    WeakThis->Reward_Icon->SetBrushFromTexture(Tex, false);
                    WeakThis->Reward_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
                }
            }
        })
    );
}

void UQuestRewardRowWidget::AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback)
{
    if (Path.IsValid())
    {
        StreamableHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(Path, Callback);
    }
}
