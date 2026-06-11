#include "UI/HarvestLootWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "UI/HarvestLootItemWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Engine/Engine.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void UHarvestLootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsVisible = false;
	CurrentLootItems.Empty();
	LootItemWidgets.Empty();
	HarvestManager = nullptr;
	HoveredItemId = -1;

	// Set up for dragging
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));

	// Set initial position (center of screen)
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		int32 W = 0, H = 0;
		PC->GetViewportSize(W, H);
		const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
		ForceLayoutPrepass();
		const FVector2D Size = GetDesiredSize();
		
		// Center the widget, clamp to viewport so it never opens off-screen
		CurrentViewportPosition = FVector2D(
			FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
			FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
		SetPositionInViewport(CurrentViewportPosition, false);
	}

	// Initialize widget but don't bind to HarvestManager yet
	InitializeWidget();

	// Bind button events
	if (Button_PickupAll)
	{
		Button_PickupAll->OnClicked.AddDynamic(this, &UHarvestLootWidget::OnPickupAllClicked);
	}

	if (Button_Close)
	{
		Button_Close->OnClicked.AddDynamic(this, &UHarvestLootWidget::OnCloseClicked);
	}

	// Create tooltip widget if class is set
	if (ItemTooltipWidgetClass)
	{
		ItemTooltipWidget = CreateWidget<UItemTooltipWidget>(this, ItemTooltipWidgetClass);
		if (ItemTooltipWidget)
		{
			ItemTooltipWidget->AddToViewport(1000); // High Z-order for tooltip
			ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UHarvestLootWidget::NativeDestruct()
{
	// Clean up tooltip
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->RemoveFromParent();
		ItemTooltipWidget = nullptr;
	}

	UnbindFromHarvestManager();
	Super::NativeDestruct();
}

void UHarvestLootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update tooltip position if visible
	if (bIsVisible && ItemTooltipWidget && ItemTooltipWidget->GetVisibility() == ESlateVisibility::HitTestInvisible)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			FVector2D Mouse;
			if (PC->GetMousePosition(Mouse.X, Mouse.Y))
			{
				ItemTooltipWidget->ForceLayoutPrepass();
				ItemTooltipWidget->UpdateTooltipPosition(Mouse);
			}
		}
	}
}

FReply UHarvestLootWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Hide tooltip when clicking anywhere in the harvest loot window
		HideTooltip();
		
		bool bShouldStartDrag = false;

		if (DragHandle)
		{
			// Check if clicking on drag handle
			const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
			const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
			
			bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
				LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);
		}
		else
		{
			// No specific drag handle, allow dragging from title area
			bShouldStartDrag = true;
		}

		if (bShouldStartDrag)
		{
			const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
			const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
			const FVector2D MouseVP = Screen / Scale;
			
			DragOffset = MouseVP - CurrentViewportPosition;
			bDragging = true;

			if (TSharedPtr<SWidget> Slate = GetCachedWidget())
			{
				return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
			}
			return FReply::Handled();
		}
	}
	
	// Hide tooltip on right mouse button click as well
	if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		HideTooltip();
	}
	
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UHarvestLootWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;

		if (TSharedPtr<SWidget> Slate = GetCachedWidget())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UHarvestLootWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UHarvestLootWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPosAbs)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D ViewportSize = FVector2D(W, H) / Scale;

	FVector2D Size = GetDesiredSize();
	if (Size.IsZero()) Size = FVector2D(400, 300);

	// Convert cursor position to viewport space
	const FVector2D MouseVP = ScreenCursorPosAbs / Scale;

	// Calculate desired position
	FVector2D Pos = MouseVP - DragOffset;

	// Clamp to viewport bounds
	Pos.X = FMath::Clamp(Pos.X, DragPadding.Left, FMath::Max(DragPadding.Left, ViewportSize.X - Size.X - DragPadding.Right));
	Pos.Y = FMath::Clamp(Pos.Y, DragPadding.Top, FMath::Max(DragPadding.Top, ViewportSize.Y - Size.Y - DragPadding.Bottom));

	// Update position
	CurrentViewportPosition = Pos;
	SetPositionInViewport(Pos, false);
}

void UHarvestLootWidget::SetHarvestManager(UHarvestManager* InHarvestManager)
{
	UnbindFromHarvestManager();
	HarvestManager = InHarvestManager;
	BindToHarvestManager();
}

void UHarvestLootWidget::BindToHarvestManager()
{
	if (HarvestManager)
	{
		HarvestManager->OnHarvestCompleted.AddDynamic(this, &UHarvestLootWidget::HandleHarvestCompleted);
		HarvestManager->OnLootPickupSuccess.AddDynamic(this, &UHarvestLootWidget::HandleLootPickupSuccess);
		HarvestManager->OnLootPickupError.AddDynamic(this, &UHarvestLootWidget::HandleLootPickupError);
	}
}

void UHarvestLootWidget::UnbindFromHarvestManager()
{
	if (HarvestManager)
	{
		HarvestManager->OnHarvestCompleted.RemoveDynamic(this, &UHarvestLootWidget::HandleHarvestCompleted);
		HarvestManager->OnLootPickupSuccess.RemoveDynamic(this, &UHarvestLootWidget::HandleLootPickupSuccess);
		HarvestManager->OnLootPickupError.RemoveDynamic(this, &UHarvestLootWidget::HandleLootPickupError);
	}
}

void UHarvestLootWidget::HandleHarvestCompleted(const FHarvestCompleteStruct& HarvestData)
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Harvest completed with %d items"), 
		HarvestData.availableLoot.Num());
		
	SetLootItems(HarvestData.availableLoot);
	
	if (HarvestData.availableLoot.Num() > 0)
	{
		ShowWidget();
		UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Showing loot window"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: No loot available, staying hidden"));
	}
}

void UHarvestLootWidget::HandleLootPickupSuccess(const FCorpseLootPickupResponseStruct& PickupData)
{
	SetLootItems(PickupData.remainingLoot);
	if (PickupData.remainingLoot.Num() == 0)
	{
		HideWidget();
	}
}

void UHarvestLootWidget::HandleLootPickupError(const FCorpseLootPickupErrorStruct& ErrorData)
{
	UpdateItemCountDisplay();
}

void UHarvestLootWidget::InitializeWidget()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: InitializeWidget called"));

	if (TextBlock_Title)
	{
		TextBlock_Title->SetText(FText::FromString(TEXT("Harvest Loot")));
	}

	UpdateItemCountDisplay();
	HideWidget();
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: InitializeWidget completed"));
}

void UHarvestLootWidget::SetLootItems(const TArray<FHarvestItemStruct>& LootItems)
{
	CurrentLootItems = LootItems;
	CreateLootItemWidgets();
	UpdateItemCountDisplay();

	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Set %d loot items"), LootItems.Num());
}

void UHarvestLootWidget::ClearLootItems()
{
	CurrentLootItems.Empty();
	
	UPanelWidget* Container = GetLootContainer();
	if (Container)
	{
		Container->ClearChildren();
	}
	
	LootItemWidgets.Empty();
	UpdateItemCountDisplay();
}

void UHarvestLootWidget::ShowWidget()
{
	if (!bIsVisible && CurrentLootItems.Num() > 0)
	{
		bIsVisible = true;
		SetVisibility(ESlateVisibility::Visible);
		
		// �� ��������� �������� ����� - ��� ������ UIManager
		// ���������� UIManager �� ��������� ���������
		OnHarvestLootVisibilityChanged.Broadcast(true);
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Widget shown - Items: %d"), CurrentLootItems.Num());
	}
}

void UHarvestLootWidget::HideWidget()
{
	bIsVisible = false;
	bDragging = false;
	SetVisibility(ESlateVisibility::Hidden);
	
	// Hide tooltip when widget is hidden
	HideTooltip();
	
	// �� ��������� �������� ����� - ��� ������ UIManager
	// ���������� UIManager �� ��������� ���������
	OnHarvestLootVisibilityChanged.Broadcast(false);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Widget hidden"));
}

void UHarvestLootWidget::UpdateRemainingLoot(const TArray<FHarvestItemStruct>& RemainingLoot)
{
	SetLootItems(RemainingLoot);
	
	if (RemainingLoot.Num() == 0)
	{
		HideWidget();
	}
}

void UHarvestLootWidget::OnPickupAllClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Pickup all clicked"));

	if (HarvestManager)
	{
		HarvestManager->PickupAllLoot();
	}
}

void UHarvestLootWidget::OnCloseClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Close clicked"));
	HideWidget();
}

void UHarvestLootWidget::OnItemPickupRequested(int32 ItemId, int32 Quantity)
{
	// Hide tooltip when item pickup is requested
	HideTooltip();
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Item pickup requested - ID: %d, Quantity: %d"), ItemId, Quantity);

	if (HarvestManager)
	{
		HarvestManager->PickupLootItem(ItemId, Quantity);
	}
}

void UHarvestLootWidget::OnItemHovered(int32 ItemId, bool bIsHovered)
{
	if (bIsHovered)
	{
		HoveredItemId = ItemId;
		
		// Find the item and show tooltip
		for (const FHarvestItemStruct& Item : CurrentLootItems)
		{
			if (Item.itemId == ItemId)
			{
				FVector2D MousePosition = FVector2D::ZeroVector;
				if (GetWorld() && GetWorld()->GetFirstPlayerController())
				{
					GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
				}
				ShowTooltip(Item, MousePosition);
				break;
			}
		}
	}
	else
	{
		if (HoveredItemId == ItemId)
		{
			HoveredItemId = -1;
			HideTooltip();
		}
	}
}

void UHarvestLootWidget::ShowTooltip(const FHarvestItemStruct& Item, FVector2D Position)
{
	if (ItemTooltipWidget)
	{
		// Convert harvest item to inventory item for tooltip
		FInventoryItemStruct InventoryItem = ConvertHarvestItemToInventoryItem(Item);
		
		ItemTooltipWidget->SetItemData(InventoryItem);
		ItemTooltipWidget->UpdateTooltipPosition(Position);
		ItemTooltipWidget->ShowTooltip();
	}
}

void UHarvestLootWidget::HideTooltip()
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->HideTooltip();
	}
}

UPanelWidget* UHarvestLootWidget::GetLootContainer() const
{
	// Use WrapBox for grid layout if enabled and available, otherwise use ScrollBox
	if (bUseGridLayout && WrapBox_LootItems)
	{
		return WrapBox_LootItems;
	}
	else if (ScrollBox_LootItems)
	{
		return ScrollBox_LootItems;
	}
	
	return nullptr;
}

void UHarvestLootWidget::CreateLootItemWidgets()
{
	UPanelWidget* Container = GetLootContainer();
	if (!Container || !LootItemWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestLootWidget: Missing required components for creating loot widgets"));
		return;
	}

	// Clear existing widgets
	Container->ClearChildren();
	LootItemWidgets.Empty();

	// Configure WrapBox if using grid layout
	if (bUseGridLayout && WrapBox_LootItems)
	{
		WrapBox_LootItems->SetInnerSlotPadding(FVector2D(SlotGap, SlotGap));
	}

	// Create widgets for each loot item
	for (const FHarvestItemStruct& LootItem : CurrentLootItems)
	{
		UHarvestLootItemWidget* ItemWidget = CreateWidget<UHarvestLootItemWidget>(GetOwningPlayer(), LootItemWidgetClass);
		if (ItemWidget)
		{
			// Set item data
			ItemWidget->SetItemData(LootItem);
			
			// Bind pickup event
			ItemWidget->OnItemPickupRequested.AddDynamic(this, &UHarvestLootWidget::OnItemPickupRequested);
			
			// Bind hover events for tooltip
			ItemWidget->OnItemHovered.AddDynamic(this, &UHarvestLootWidget::OnItemHovered);
			
			// Add to container
			if (bUseGridLayout && WrapBox_LootItems)
			{
				// Add to WrapBox with slot configuration
				if (UWrapBoxSlot* WrapSlot = WrapBox_LootItems->AddChildToWrapBox(ItemWidget))
				{
					WrapSlot->SetPadding(FMargin(SlotGap));
					WrapSlot->SetHorizontalAlignment(HAlign_Left);
					WrapSlot->SetVerticalAlignment(VAlign_Top);
					WrapSlot->SetFillEmptySpace(false);
				}
			}
			else if (ScrollBox_LootItems)
			{
				// Add to ScrollBox
				ScrollBox_LootItems->AddChild(ItemWidget);
			}
			
			LootItemWidgets.Add(ItemWidget);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Created %d loot item widgets using %s layout"), 
		LootItemWidgets.Num(), bUseGridLayout ? TEXT("Grid") : TEXT("List"));
}

void UHarvestLootWidget::UpdateItemCountDisplay()
{
	if (TextBlock_ItemCount)
	{
		FString CountText = FString::Printf(TEXT("Items: %d"), CurrentLootItems.Num());
		TextBlock_ItemCount->SetText(FText::FromString(CountText));
	}

	// Enable/disable pickup all button based on item count
	if (Button_PickupAll)
	{
		Button_PickupAll->SetIsEnabled(CurrentLootItems.Num() > 0);
	}
}

FInventoryItemStruct UHarvestLootWidget::ConvertHarvestItemToInventoryItem(const FHarvestItemStruct& HarvestItem) const
{
	FInventoryItemStruct InventoryItem;
	
	InventoryItem.itemId = HarvestItem.itemId;
	InventoryItem.slug = HarvestItem.itemSlug;
	InventoryItem.quantity = HarvestItem.quantity;
	InventoryItem.name = HarvestItem.name;
	InventoryItem.description = HarvestItem.description;
	InventoryItem.type = HarvestItem.itemType;
	InventoryItem.weight = HarvestItem.weight;
	
	// Convert rarity from ID to string
	switch (HarvestItem.rarityId)
	{
		case 1: InventoryItem.rarity = TEXT("common"); break;
		case 2: InventoryItem.rarity = TEXT("uncommon"); break;
		case 3: InventoryItem.rarity = TEXT("rare"); break;
		case 4: InventoryItem.rarity = TEXT("epic"); break;
		case 5: InventoryItem.rarity = TEXT("legendary"); break;
		default: InventoryItem.rarity = TEXT("common"); break;
	}
	
	return InventoryItem;
}