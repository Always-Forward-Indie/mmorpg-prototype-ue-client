#include "UI/VendorTooltipWidget.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/AssetManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

FLinearColor UVendorTooltipWidget::GetRarityColor(const FString& RaritySlug) const
{
    const FString Key = RaritySlug.ToLower();
    if (const FLinearColor* Found = RarityColors.Find(Key))
        return *Found;
    return FLinearColor::White;
}

void UVendorTooltipWidget::LoadIconBySlug(const FString& Slug)
{
    if (!ItemIcon || Slug.IsEmpty()) return;

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr);
    if (!GI) return;

    UItemManager* ItemMgr = GI->GetItemManager();
    if (!ItemMgr) return;

    FItemVisualData VisualData = ItemMgr->GetItemVisualDataBySlug(Slug);
    if (VisualData.Icon.IsNull())
    {
        ItemIcon->SetBrushFromTexture(nullptr);
        return;
    }

    if (UTexture2D* Already = VisualData.Icon.Get())
    {
        SetIconTexture(Already);
        return;
    }

    TWeakObjectPtr<UVendorTooltipWidget> WeakThis = this;
    TSoftObjectPtr<UTexture2D> SoftIcon = VisualData.Icon;
    AsyncLoad(SoftIcon.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, SoftIcon]()
        {
            if (WeakThis.IsValid())
                if (UTexture2D* Tex = SoftIcon.Get())
                    WeakThis->SetIconTexture(Tex);
        })
    );
}

void UVendorTooltipWidget::SetIconTexture(UTexture2D* Texture)
{
    if (ItemIcon && Texture)
    {
        ItemIcon->SetBrushFromTexture(Texture);
        ItemIcon->SetColorAndOpacity(FLinearColor::White);
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }
}

void UVendorTooltipWidget::AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback)
{
    if (AssetPath.IsValid())
    {
        StreamableHandle = UAssetManager::Get().GetStreamableManager()
            .RequestAsyncLoad(AssetPath, Callback);
    }
}
