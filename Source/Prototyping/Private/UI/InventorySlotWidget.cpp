#include "UI/InventorySlotWidget.h"
#include "Prototyping.h"
#include "Utils/NetCore/CooldownFormat.h"
#include "Utils/NetCore/InventorySlotStyleRule.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"

UInventorySlotWidget::UInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SlotIndex = -1;
	bIsHighlighted = false;
	bIsSelected = false;
	bIsHovered = false;
	bIsOccupied = false;
	CurrentItem = FInventoryItemStruct();
}

void UInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Single input path: everything rides the button. Right-click stays in
	// NativeOnMouseButtonDown below (UButton only fires left-click).
	if (SlotButton)
	{
		SlotButton->OnClicked.AddDynamic(this, &UInventorySlotWidget::OnSlotButtonClicked);
		SlotButton->OnHovered.AddDynamic(this, &UInventorySlotWidget::OnSlotButtonHovered);
		SlotButton->OnUnhovered.AddDynamic(this, &UInventorySlotWidget::OnSlotButtonUnhovered);
	}

	// Load-bearing binds: fail loud, never silent (UI_Widget_BP_Bindings §3.2).
	// A missing SlotButton kills all input; a missing icon kills the item visual.
	if (!SlotButton)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: SlotButton bind missing in %s (slot %d) — slot input is dead. Add a UButton named 'SlotButton' filling the slot."), *GetName(), SlotIndex);
	}
	if (!ItemIcon)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: ItemIcon bind missing in %s (slot %d) — items have no icon. Add a UImage named 'ItemIcon'."), *GetName(), SlotIndex);
	}
	if (SlotButton && SlotButton->GetStyle().Normal.GetResourceObject() == nullptr && !HasAnyPresetStyle())
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventorySlotWidget: %s (slot %d) has no button visuals — set the button Style brushes or the Empty/Filled/Hovered/Selected/Highlighted presets, or empty slots are invisible."), *GetName(), SlotIndex);
	}

	// Structural size: a content-less button collapses to 0x0 (dead input),
	// so the cell size must never depend on content. Enforce both halves.
	EnforceButtonFill();

	if (CooldownOverlay)
		CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if (CooldownText)
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);

	// Initialize visual appearance
	UpdateSlotAppearance();
}

void UInventorySlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// One-shot button sanity once the slot is laid out in an open window.
	// Skipped silently while any ancestor is collapsed/hidden (window closed).
	if (!bButtonSanityDone)
	{
		SanityAccum += InDeltaTime;
		if (SanityAccum >= 1.0f)
		{
			SanityAccum = 0.0;
			if (!IsAncestorChainHidden())
			{
				bButtonSanityDone = true;
				RunButtonSanityCheck();
			}
		}
	}

	if (!bCoolingDown) return;
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now >= CooldownEndTime)
	{
		bCoolingDown = false;
		CooldownTotalTime = 0.0;
		if (CooldownOverlay)
			CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
		if (CooldownText)
			CooldownText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UpdateCooldownDisplay();
	}
}

void UInventorySlotWidget::StartCooldown(float CooldownSeconds)
{
	if (CooldownSeconds <= 0.f || !HasItem()) return;
	if (UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		CooldownEndTime = Now + static_cast<double>(CooldownSeconds);
		CooldownTotalTime = CooldownSeconds;
		bCoolingDown = true;
		UpdateCooldownDisplay();
	}
}

bool UInventorySlotWidget::IsOnCooldown() const
{
	return bCoolingDown;
}

void UInventorySlotWidget::UpdateCooldownDisplay()
{
	if (!CooldownOverlay && !CooldownText) return;
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Remaining = static_cast<float>(FMath::Max(0.0, CooldownEndTime - Now));
	if (CooldownOverlay)
	{
		CooldownOverlay->SetVisibility(Remaining > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CooldownText)
	{
		if (Remaining > 0.0f)
		{
			CooldownText->SetText(FText::FromString(FCooldownFormat::Format(Remaining)));
			CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Hide tooltip on any mouse button click
	OnSlotHovered.Broadcast(SlotIndex, false);

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnSlotRightClicked.Broadcast(SlotIndex);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlotWidget::InitializeSlot(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	ClearSlot();

	UE_LOG(LogGameplay, Verbose, TEXT("InventorySlotWidget: Initialized slot %d"), SlotIndex);
}

void UInventorySlotWidget::SetItemData(const FInventoryItemStruct& Item)
{
	CurrentItem = Item;
	bIsOccupied = (CurrentItem.itemId > 0);

	// Update visual components
	LoadItemIcon();
	UpdateQuantityText();
	UpdateSlotAppearance();

	UE_LOG(LogGameplay, Verbose, TEXT("InventorySlotWidget: Set item data for slot %d - %s"), SlotIndex, *Item.name);
}

void UInventorySlotWidget::ClearSlot()
{
	CurrentItem = FInventoryItemStruct();
	bIsOccupied = false;
	bIsSelected = false;
	bIsJunked = false;
	bCoolingDown = false;
	CooldownEndTime = 0.0;
	CooldownTotalTime = 0.0;
	if (CooldownOverlay)
		CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if (CooldownText)
		CooldownText->SetVisibility(ESlateVisibility::Collapsed);

	// Clear visual components
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(nullptr);
		ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::GetEmpty());
		QuantityText->SetVisibility(ESlateVisibility::Hidden);
	}

	UpdateSlotAppearance();
}

void UInventorySlotWidget::SetSlotHighlight(bool bHighlighted)
{
	bIsHighlighted = bHighlighted;
	UpdateSlotAppearance();
}

void UInventorySlotWidget::SetJunked(bool bJunked)
{
	bIsJunked = bJunked;
	if (ItemIcon && HasItem())
		ItemIcon->SetColorAndOpacity(bIsJunked ? FLinearColor(0.45f, 0.45f, 0.45f, 1.f) : FLinearColor::White);
}

void UInventorySlotWidget::SetSlotSelected(bool bSelected)
{
	bIsSelected = bSelected;
	UpdateSlotAppearance();
}

void UInventorySlotWidget::UpdateSlotAppearance()
{
	const bool bHasItem = HasItem();
	const FString FrameKey = FInventorySlotStyleRule::FrameKey(bHasItem, bIsSelected, bIsHighlighted, bIsHovered);

	// Frame chrome: one button style per state. Unset presets are skipped so a
	// BP predating them keeps its authored style instead of going invisible.
	if (SlotButton)
	{
		if (const FButtonStyle* Style = StyleForKey(FrameKey))
		{
			SlotButton->SetStyle(*Style);
		}
	}

	// Rarity edge: tinted only on occupied slots, hidden otherwise.
	if (RarityBorder)
	{
		if (bHasItem)
		{
			const FString& RarityKey = CurrentItem.raritySlug.IsEmpty() ? CurrentItem.rarity : CurrentItem.raritySlug;
			RarityBorder->SetBrushColor(FInventorySlotStyleRule::RarityColor(RarityKey));
			RarityBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			RarityBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Equipped badge survives as the single overlay image.
	if (ImgEquipped)
	{
		ImgEquipped->SetVisibility(
			(bHasItem && CurrentItem.is_equipped)
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

const FButtonStyle* UInventorySlotWidget::StyleForKey(const FString& Key) const
{
	const FButtonStyle* Candidate = nullptr;
	if (Key == TEXT("highlighted"))      Candidate = &HighlightedStyle;
	else if (Key == TEXT("selected"))   Candidate = &SelectedStyle;
	else if (Key == TEXT("hovered"))    Candidate = &HoveredStyle;
	else if (Key == TEXT("filled"))     Candidate = &FilledStyle;
	else                                Candidate = &EmptyStyle;

	// Empty Normal brush = preset never assigned in BP: keep authored style.
	if (Candidate && Candidate->Normal.GetResourceObject() != nullptr) return Candidate;
	return nullptr;
}

bool UInventorySlotWidget::HasAnyPresetStyle() const
{
	const FButtonStyle* Presets[] = { &EmptyStyle, &FilledStyle, &HoveredStyle, &SelectedStyle, &HighlightedStyle };
	for (const FButtonStyle* P : Presets)
	{
		if (P && P->Normal.GetResourceObject() != nullptr) return true;
	}
	return false;
}

bool UInventorySlotWidget::IsAncestorChainHidden() const
{
	for (const UWidget* P = GetParent(); P != nullptr; P = P->GetParent())
	{
		const ESlateVisibility V = P->GetVisibility();
		if (V == ESlateVisibility::Collapsed || V == ESlateVisibility::Hidden) return true;
	}
	return false;
}

void UInventorySlotWidget::EnforceButtonFill()
{
	// Half 1: the cell size override. Silent when missing before — now loud:
	// without the SizeBox nothing enforces the 64px cell and SetSlotSide is
	// a no-op, so an empty button collapses to 0x0 (see sanity check below).
	if (SlotSizeBox)
	{
		SetSlotSide(SlotSide);
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: SlotSizeBox bind missing in %s (slot %d) — cell size is unenforced and an empty button collapses to 0x0 (dead input). Add a USizeBox named 'SlotSizeBox' as the slot root with Width/Height override."), *GetName(), SlotIndex);
	}

	// Half 2: the button must fill whatever cell it sits in (contract, not
	// styling — a smaller button shrinks the only input path). Idempotent:
	// re-asserting Fill on a compliant BP changes nothing.
	if (!SlotButton || !SlotButton->Slot) return;
	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(SlotButton->Slot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(FMargin(0.f));
	}
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SlotButton->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
	}
}

void UInventorySlotWidget::RunButtonSanityCheck()
{
	// Missing bind is already an Error from NativeConstruct.
	if (!SlotButton) return;
	if (!SlotButton->GetIsEnabled())
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: %s (slot %d) button is disabled — slot input is dead. Check IsEnabled on SlotButton (and parents)."), *GetName(), SlotIndex);
		return;
	}
	if (SlotButton->GetVisibility() != ESlateVisibility::Visible)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: %s (slot %d) button visibility is not Visible — slot input is dead. SlotButton must be Visible (never Hidden/Collapsed/HitTestInvisible)."), *GetName(), SlotIndex);
		return;
	}
	const FVector2D Size = SlotButton->GetCachedGeometry().GetLocalSize();
	if (Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: %s (slot %d) button has zero size (%.0fx%.0f) — slot input is dead. SlotButton must fill the slot."), *GetName(), SlotIndex, Size.X, Size.Y);
	}
}

void UInventorySlotWidget::LoadItemIcon()
{
	if (!ItemIcon || !HasItem())
	{
		return;
	}

	// Get ItemManager from game instance
	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: GameInstance not found"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotWidget: ItemManager not found"));
		return;
	}

	// Get visual data for current item using its slug
	FItemVisualData VisualData = ItemManager->GetItemVisualDataBySlug(CurrentItem.slug);

	if (!VisualData.Icon.IsNull())
	{
		// Load the icon texture asynchronously
		TSoftObjectPtr<UTexture2D> IconSoftPtr = VisualData.Icon;

		// Check if already loaded
		if (UTexture2D* LoadedTexture = IconSoftPtr.Get())
		{
			// Already loaded, set immediately
			SetIconTexture(LoadedTexture);
		}
		else
		{
			// Load asynchronously (guarded against grid rebuilds; see below)
			TWeakObjectPtr<UInventorySlotWidget> WeakThis = this;
			const int32 ExpectedInstanceId = CurrentItem.id;
			const int32 ExpectedTemplateId = CurrentItem.itemId;

			AsyncLoad(IconSoftPtr.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, IconSoftPtr, ExpectedInstanceId, ExpectedTemplateId]()
					{
						if (WeakThis.IsValid())
						{
							const FInventoryItemStruct& NowShowing = WeakThis->GetItemData();
							if (NowShowing.id != ExpectedInstanceId || NowShowing.itemId != ExpectedTemplateId)
							{
								return; // Slot moved on; its own load will paint it.
							}
							if (UTexture2D* LoadedTexture = IconSoftPtr.Get())
							{
								WeakThis->SetIconTexture(LoadedTexture);
							}
						}
					})
			);
		}
	}
	else
	{
		// No icon found, use default or placeholder
		SetDefaultIcon();
	}

	// Show icon (HitTestInvisible: decorations must never steal the SlotButton hit).
	ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

	UE_LOG(LogGameplay, Verbose, TEXT("InventorySlotWidget: Loading icon for item %s (ID: %d)"), *CurrentItem.name, CurrentItem.itemId);
}

void UInventorySlotWidget::SetIconTexture(UTexture2D* Texture)
{
	if (ItemIcon && Texture)
	{
		ItemIcon->SetBrushFromTexture(Texture);
		ItemIcon->SetColorAndOpacity(bIsJunked ? FLinearColor(0.45f, 0.45f, 0.45f, 1.f) : FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		ItemIcon->SetRenderOpacity(1.0f);

		UE_LOG(LogGameplay, Verbose, TEXT("Set icon texture %s. Has alpha: %s"), *Texture->GetName(), Texture->HasAlphaChannel() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogGameplay, Verbose, TEXT("InventorySlotWidget: Set icon texture for item %s"), *CurrentItem.name);
	}
}

void UInventorySlotWidget::SetDefaultIcon()
{
	if (ItemIcon)
	{
		// Set a default placeholder icon or clear the brush
		ItemIcon->SetBrushFromTexture(nullptr);
		// Or use a default placeholder texture if you have one
		// ItemIcon->SetBrushFromTexture(DefaultPlaceholderTexture);
	}
}

// Helper function for async loading
void UInventorySlotWidget::AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback)
{
	if (AssetPath.IsValid())
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TSharedPtr<FStreamableHandle> Handle = AssetManager.GetStreamableManager().RequestAsyncLoad(
			AssetPath,
			Callback
		);

		// Store the handle if you need to cancel loading later
		StreamableHandle = Handle;
	}
}

void UInventorySlotWidget::UpdateQuantityText()
{
	if (!QuantityText)
	{
		return;
	}

	if (HasItem() && CurrentItem.quantity > 1)
	{
		QuantityText->SetText(FText::AsNumber(CurrentItem.quantity));
		QuantityText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		QuantityText->SetText(FText::GetEmpty());
		QuantityText->SetVisibility(ESlateVisibility::Hidden);
	}
}

FLinearColor UInventorySlotWidget::GetRarityColor(const FString& Rarity) const
{
	// Single source: the NetCore style rule (no forked tables).
	return FInventorySlotStyleRule::RarityColor(Rarity);
}

void UInventorySlotWidget::OnSlotButtonClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}

void UInventorySlotWidget::OnSlotButtonHovered()
{
	bIsHovered = true;
	UpdateSlotAppearance();
	OnSlotHovered.Broadcast(SlotIndex, true);
}

void UInventorySlotWidget::OnSlotButtonUnhovered()
{
	bIsHovered = false;
	UpdateSlotAppearance();
	OnSlotHovered.Broadcast(SlotIndex, false);
}

void UInventorySlotWidget::SetSlotSide(float InSide)
{
	SlotSide = InSide;
	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(SlotSide);
		SlotSizeBox->SetHeightOverride(SlotSide);
	}
}

void UInventorySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetSlotSide(SlotSide);
}
