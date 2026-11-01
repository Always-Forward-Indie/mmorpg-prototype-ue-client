#include "UI/ItemQuickSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/ItemStruct.h"
#include "MyGameInstance.h"

void UItemQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ItemButton)
		ItemButton->OnClicked.AddDynamic(this, &UItemQuickSlotWidget::OnSlotClicked);

	if (CooldownOverlay)
		CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);

	if (QuantityText)
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);

	if (ItemIcon)
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
}

void UItemQuickSlotWidget::Setup(UInventoryManager* InInventoryManager, int32 InSlotIndex, const FKey& InBoundKey)
{
	InventoryManager = InInventoryManager;
	SlotData.slotIndex = InSlotIndex;
	SlotData.boundKey = InBoundKey;

	if (HotkeyText)
		HotkeyText->SetText(FText::FromString(InBoundKey.ToString()));
}

void UItemQuickSlotWidget::AssignItem(int32 ItemId, const FString& ItemSlug, int32 Quantity, const FString& InIconPath)
{
	SlotData.itemId = ItemId;
	SlotData.itemSlug = ItemSlug;
	SlotData.quantity = Quantity;
	SlotData.iconPath = InIconPath;

	if (!InIconPath.IsEmpty())
	{
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *InIconPath);
		if (ItemIcon && Tex)
		{
			ItemIcon->SetBrushFromTexture(Tex);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	else if (!ItemSlug.IsEmpty())
	{
		// No cached path (menu-assigned items): resolve through the item
		// catalog exactly like inventory slots do.
		LoadIconBySlug(ItemSlug, ItemId);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
		QuantityText->SetVisibility(Quantity > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EmptySlotIcon)
		EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);
}

void UItemQuickSlotWidget::ClearSlot()
{
	SlotData.itemId = 0;
	SlotData.itemSlug.Empty();
	SlotData.quantity = 0;
	SlotData.iconPath.Empty();

	if (ItemIcon)
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);

	if (QuantityText)
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);

	if (EmptySlotIcon)
		EmptySlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UItemQuickSlotWidget::UpdateQuantity(int32 NewQuantity)
{
	SlotData.quantity = NewQuantity;

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(NewQuantity));
		QuantityText->SetVisibility(NewQuantity > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (NewQuantity <= 0)
		ClearSlot();
}

void UItemQuickSlotWidget::UseItem()
{
	if (SlotData.itemId <= 0 || !InventoryManager) return;

	InventoryManager->UseItem(SlotData.itemId, 1);
	OnQuickSlotUsed.Broadcast(SlotData.slotIndex);
}

void UItemQuickSlotWidget::OnSlotClicked()
{
	UseItem();
}

void UItemQuickSlotWidget::StartCooldown(float CooldownSeconds)
{
	if (CooldownSeconds <= 0.f || SlotData.itemId <= 0) return;
	if (UWorld* World = GetWorld())
	{
		CooldownEndTime = World->GetTimeSeconds() + static_cast<double>(CooldownSeconds);
		bCoolingDown = true;
		if (CooldownOverlay)
			CooldownOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

bool UItemQuickSlotWidget::IsOnCooldown() const
{
	return bCoolingDown;
}

void UItemQuickSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bCoolingDown) return;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now >= CooldownEndTime)
	{
		bCoolingDown = false;
		if (CooldownOverlay)
			CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UItemQuickSlotWidget::LoadIconBySlug(const FString& Slug, int32 ExpectedItemId)
{
	if (!ItemIcon) return;
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI) return;
	UItemManager* ItemMgr = GI->GetItemManager();
	if (!ItemMgr) return;

	const FItemVisualData Vis = ItemMgr->GetItemVisualDataBySlug(Slug);
	if (Vis.Icon.IsNull()) return;

	if (UTexture2D* Loaded = Vis.Icon.Get())
	{
		SetIconTexture(Loaded);
		return;
	}

	TWeakObjectPtr<UItemQuickSlotWidget> WeakThis(this);
	const FSoftObjectPath Path = Vis.Icon.ToSoftObjectPath();
	TSoftObjectPtr<UTexture2D> IconSoft = Vis.Icon;
	if (UAssetManager* AM = UAssetManager::GetIfInitialized())
	{
		StreamableHandle = AM->GetStreamableManager().RequestAsyncLoad(Path,
			FStreamableDelegate::CreateLambda([WeakThis, IconSoft, ExpectedItemId]()
			{
				if (!WeakThis.IsValid() || WeakThis->GetItemId() != ExpectedItemId) return;
				if (UTexture2D* Tex = IconSoft.Get())
					WeakThis->SetIconTexture(Tex);
			}));
	}
}

void UItemQuickSlotWidget::SetIconTexture(UTexture2D* Texture)
{
	if (ItemIcon && Texture)
	{
		ItemIcon->SetBrushFromTexture(Texture);
		ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

FReply UItemQuickSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !IsEmpty())
	{
		ClearSlot();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
