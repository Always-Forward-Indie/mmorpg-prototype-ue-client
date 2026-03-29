#include "UI/EquipmentSlotWidget.h"
#include "Gameplay/Items/ItemManager.h"
#include "MyGameInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UEquipmentSlotWidget::UEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UEquipmentSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this,   &UEquipmentSlotWidget::HandleSlotButtonClicked);
        SlotButton->OnHovered.AddDynamic(this,   &UEquipmentSlotWidget::HandleSlotButtonHovered);
        SlotButton->OnUnhovered.AddDynamic(this, &UEquipmentSlotWidget::HandleSlotButtonUnhovered);
    }

    ClearSlot();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UEquipmentSlotWidget::InitializeSlot(const FString& InSlotSlug, const FString& InSlotDisplayName)
{
    SlotSlug        = InSlotSlug;
    SlotDisplayName = InSlotDisplayName;

    if (SlotNameText)
        SlotNameText->SetText(FText::FromString(InSlotDisplayName));

    ClearSlot();
}

void UEquipmentSlotWidget::SetSlotData(const FEquipmentSlotData& SlotData)
{
    CachedSlotData = SlotData;
    UpdateVisuals();
    RefreshIcon();
}

void UEquipmentSlotWidget::ClearSlot()
{
    CachedSlotData = FEquipmentSlotData{};
    UpdateVisuals();

    if (ItemIcon)
        SetDefaultIcon();
}

void UEquipmentSlotWidget::RefreshIcon()
{
    if (!ItemIcon) return;

    if (!CachedSlotData.bIsOccupied || CachedSlotData.itemSlug.IsEmpty())
    {
        SetDefaultIcon();
        return;
    }

    UMyGameInstance* GI = GetWorld() ? Cast<UMyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
    if (!GI) { SetDefaultIcon(); return; }

    UItemManager* ItemMgr = GI->GetItemManager();
    if (!ItemMgr) { SetDefaultIcon(); return; }

    FItemVisualData VisualData = ItemMgr->GetItemVisualDataBySlug(CachedSlotData.itemSlug);

    if (VisualData.Icon.IsNull())
    {
        SetDefaultIcon();
        return;
    }

    if (UTexture2D* Loaded = VisualData.Icon.Get())
    {
        SetIconTexture(Loaded);
        return;
    }

    TWeakObjectPtr<UEquipmentSlotWidget> WeakThis = this;
    TSoftObjectPtr<UTexture2D> SoftIcon = VisualData.Icon;
    AsyncLoad(SoftIcon.ToSoftObjectPath(), FStreamableDelegate::CreateLambda(
        [WeakThis, SoftIcon]()
        {
            if (WeakThis.IsValid())
            {
                if (UTexture2D* Tex = SoftIcon.Get())
                    WeakThis->SetIconTexture(Tex);
                else
                    WeakThis->SetDefaultIcon();
            }
        }));
}

// ---------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------

void UEquipmentSlotWidget::UpdateVisuals()
{
    const bool bOccupied  = CachedSlotData.bIsOccupied;
    const bool bBlocked   = CachedSlotData.blockedByTwoHanded;

    // Occupied / empty images
    if (ImgOccupied)
        ImgOccupied->SetVisibility(bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (ImgEmpty)
        ImgEmpty->SetVisibility(bOccupied ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

    // Blocked overlay (off_hand locked by two-handed weapon)
    if (BlockedOverlay)
        BlockedOverlay->SetVisibility(bBlocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // Item icon tint
    if (ItemIcon)
    {
        if (bBlocked)
            ItemIcon->SetColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
        else if (bOccupied)
            ItemIcon->SetColorAndOpacity(ColorOccupied);
        else
            ItemIcon->SetColorAndOpacity(ColorEmpty);
    }

    // Durability bar
    UpdateDurabilityBar();

    // Hover stroke
    if (ImgStroke)
        ImgStroke->SetVisibility(bIsHovered && bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UEquipmentSlotWidget::UpdateDurabilityBar()
{
    if (!DurabilityBar) return;

    if (!CachedSlotData.bIsOccupied || CachedSlotData.durabilityMax <= 0)
    {
        DurabilityBar->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const float Ratio = FMath::Clamp(
        static_cast<float>(CachedSlotData.durabilityCurrent) / static_cast<float>(CachedSlotData.durabilityMax),
        0.f, 1.f);

    DurabilityBar->SetPercent(Ratio);
    DurabilityBar->SetVisibility(ESlateVisibility::Visible);

    // Color by durability level
    FLinearColor BarColor;
    if (CachedSlotData.isDurabilityWarning || Ratio <= DurabilityWarningThreshold)
        BarColor = (Ratio <= 0.1f) ? ColorDurabilityLow : ColorDurabilityWarn;
    else
        BarColor = ColorDurabilityOk;

    DurabilityBar->SetFillColorAndOpacity(BarColor);
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void UEquipmentSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    bIsHovered = true;
    UpdateVisuals();
    OnEquipSlotHovered.Broadcast(SlotSlug, true);
}

void UEquipmentSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    bIsHovered = false;
    UpdateVisuals();
    OnEquipSlotHovered.Broadcast(SlotSlug, false);
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        OnEquipSlotRightClicked.Broadcast(SlotSlug);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// ---------------------------------------------------------------------------
// Button delegates
// ---------------------------------------------------------------------------

void UEquipmentSlotWidget::HandleSlotButtonClicked()
{
    OnEquipSlotClicked.Broadcast(SlotSlug);
}

void UEquipmentSlotWidget::HandleSlotButtonHovered()
{
    bIsHovered = true;
    UpdateVisuals();
    OnEquipSlotHovered.Broadcast(SlotSlug, true);
}

void UEquipmentSlotWidget::HandleSlotButtonUnhovered()
{
    bIsHovered = false;
    UpdateVisuals();
    OnEquipSlotHovered.Broadcast(SlotSlug, false);
}

// ---------------------------------------------------------------------------
// Icon helpers
// ---------------------------------------------------------------------------

void UEquipmentSlotWidget::SetIconTexture(UTexture2D* Texture)
{
    if (ItemIcon && Texture)
    {
        ItemIcon->SetBrushFromTexture(Texture);
        ItemIcon->SetColorAndOpacity(ColorOccupied);
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }
}

void UEquipmentSlotWidget::SetDefaultIcon()
{
    if (ItemIcon)
    {
        ItemIcon->SetBrushFromTexture(nullptr);
        ItemIcon->SetColorAndOpacity(ColorEmpty);
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }
}

void UEquipmentSlotWidget::AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback)
{
    if (!Path.IsValid())
    {
        if (Callback.IsBound()) Callback.Execute();
        return;
    }
    if (Path.ResolveObject())
    {
        if (Callback.IsBound()) Callback.Execute();
        return;
    }

    if (UAssetManager* AM = UAssetManager::GetIfInitialized())
    {
        StreamableHandle = AM->GetStreamableManager().RequestAsyncLoad(Path, MoveTemp(Callback));
    }
    else
    {
        static FStreamableManager StaticSM;
        StreamableHandle = StaticSM.RequestAsyncLoad(Path, MoveTemp(Callback));
    }
}
