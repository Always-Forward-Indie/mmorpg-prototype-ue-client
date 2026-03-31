#include "UI/InventoryWidget.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Components/GridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InventoryManager = nullptr;
	ItemTooltipWidget = nullptr;
	bIsInventoryVisible = false;
	HoveredSlotIndex = -1;
	CurrentInventory = FCharacterInventoryStruct();
	MaxInventoryWeight = 100.0f;
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();


	// Чтобы окно можно было позиционировать
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f)); // абсолютная позиция
	SetAlignmentInViewport(FVector2D(0.f, 0.f));        // левый-верх как опорная точка

	// Начальная позиция - правый нижний угол с отступом
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		int32 W = 0, H = 0;
		PC->GetViewportSize(W, H);
		ForceLayoutPrepass();
		const FVector2D Size = GetDesiredSize();

	//	// Отступ от краев экрана
		const float Margin = 100.0f;

	//	// Позиция в правом нижнем углу: справа (W - ширина виджета - отступ), снизу (H - высота виджета - отступ)
		CurrentViewportPosition = FVector2D(W - Size.X - Margin, H - Size.Y - Margin);

		UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: Setting initial position to bottom-right: %s (Viewport: %dx%d, Size: %s)"),
			*CurrentViewportPosition.ToString(), W, H, *Size.ToString());

		SetPositionInViewport(CurrentViewportPosition, false);
	}

	//CurrentViewportPosition = FVector2D::ZeroVector;


	// Bind close button event
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::OnCloseButtonClicked);
	}

	// Create initial inventory grid
	CreateInventoryGrid();

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

	// Set initial visibility
	SetInventoryVisible(bIsInventoryVisible);
}

void UInventoryWidget::NativeDestruct()
{
	// Clean up tooltip
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->RemoveFromParent();
		ItemTooltipWidget = nullptr;
	}

	// Unbind from inventory manager
	if (InventoryManager)
	{
		InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &UInventoryWidget::OnInventoryUpdated);
		InventoryManager->OnItemAdded.RemoveDynamic(this, &UInventoryWidget::OnItemAdded);
		InventoryManager->OnItemRemoved.RemoveDynamic(this, &UInventoryWidget::OnItemRemoved);
	}

	Super::NativeDestruct();
}

void UInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// если инвентарь скрыт — выходим
	if (!bIsInventoryVisible || !ItemTooltipWidget) return;

	// тултип реально виден?
	const ESlateVisibility Vis = ItemTooltipWidget->GetVisibility();
	const bool bTooltipVisible = (Vis == ESlateVisibility::HitTestInvisible ||
		Vis == ESlateVisibility::SelfHitTestInvisible) &&
		ItemTooltipWidget->GetRenderOpacity() > 0.f;
	if (!bTooltipVisible) return;

	// получаем позицию мыши и двигаем тултип
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FVector2D Mouse;
		if (PC->GetMousePosition(Mouse.X, Mouse.Y))
		{
			// на всякий случай — перед расчетом размера
			ItemTooltipWidget->ForceLayoutPrepass();
			ItemTooltipWidget->UpdateTooltipPosition(Mouse);
		}
	}
}

void UInventoryWidget::InitializeInventory(UInventoryManager* InInventoryManager)
{
	if (!InInventoryManager)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryWidget: Cannot initialize with null InventoryManager"));
		return;
	}

	// Unbind from previous manager if any
	if (InventoryManager)
	{
		InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &UInventoryWidget::OnInventoryUpdated);
		InventoryManager->OnItemAdded.RemoveDynamic(this, &UInventoryWidget::OnItemAdded);
		InventoryManager->OnItemRemoved.RemoveDynamic(this, &UInventoryWidget::OnItemRemoved);
	}

	InventoryManager = InInventoryManager;

	// Bind to inventory manager events
	InventoryManager->OnInventoryUpdated.AddDynamic(this, &UInventoryWidget::OnInventoryUpdated);
	InventoryManager->OnItemAdded.AddDynamic(this, &UInventoryWidget::OnItemAdded);
	InventoryManager->OnItemRemoved.AddDynamic(this, &UInventoryWidget::OnItemRemoved);

	// Update display with current inventory
	if (InventoryManager->IsInventoryLoaded())
	{
		UpdateInventoryDisplay(InventoryManager->GetInventory());
	}

	UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: Successfully initialized with InventoryManager"));
}

void UInventoryWidget::UpdateInventoryDisplay(const FCharacterInventoryStruct& Inventory)
{
	CurrentInventory = Inventory;

	// Clear all slots first
	for (int32 i = 0; i < InventorySlots.Num(); i++)
	{
		ClearSlot(i);
	}

	// Fill slots with items
	for (int32 i = 0; i < Inventory.items.Num() && i < InventorySlots.Num(); i++)
	{
		UpdateSlot(i, Inventory.items[i]);
	}

	// Update inventory statistics
	UpdateInventoryStats();

	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Updated display with %d items"), Inventory.items.Num());
}

void UInventoryWidget::SetInventorySize(int32 Rows, int32 Columns)
{
	InventoryRows = FMath::Max(1, Rows);
	InventoryColumns = FMath::Max(1, Columns);

	// Recreate grid with new size
	CreateInventoryGrid();

	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Set inventory size to %dx%d"), InventoryRows, InventoryColumns);
}

void UInventoryWidget::SetInventoryVisible(bool bVisible)
{
	bIsInventoryVisible = bVisible;
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

	if (!bVisible)
	{
		bDragging = false; // <-- сбрасываем drag
		HideTooltip();
	}

	// НЕ управляем курсором здесь - это делает UIManager
	// Уведомляем UIManager об изменении видимости
	OnInventoryVisibilityChanged.Broadcast(bVisible);

	// Hide tooltip when inventory is hidden
	if (!bVisible)
	{
		HideTooltip();
	}

	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Set visibility to %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UInventoryWidget::ToggleInventory()
{
	SetInventoryVisible(!bIsInventoryVisible);
}

bool UInventoryWidget::IsInventoryVisible() const
{
	return bIsInventoryVisible;
}

void UInventoryWidget::CreateInventoryGrid()
{
	if (!InventoryWrap || !InventorySlotWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryWidget: Cannot create wrap - missing components"));
		return;
	}

	ClearInventoryGrid();

	// Настройки WrapBox
	InventoryWrap->SetExplicitWrapSize(false); // = Use Allotted Width
	InventoryWrap->SetInnerSlotPadding(FVector2D(SlotGap, SlotGap)); // отступы между ячейками

	const int32 TotalSlots = GetTotalSlots();
	InventorySlots.Reserve(TotalSlots);

	for (int32 Index = 0; Index < TotalSlots; ++Index)
	{
		UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, InventorySlotWidgetClass);
		if (!SlotWidget) continue;

		SlotWidget->InitializeSlot(Index);

		SlotWidget->OnSlotClicked.AddDynamic(this, &UInventoryWidget::OnSlotClicked);
		SlotWidget->OnSlotRightClicked.AddDynamic(this, &UInventoryWidget::OnSlotRightClicked);
		SlotWidget->OnSlotHovered.AddDynamic(this, &UInventoryWidget::OnSlotHovered);

		if (UWrapBoxSlot* W = InventoryWrap->AddChildToWrapBox(SlotWidget))
		{
			W->SetPadding(FMargin(SlotGap));
			W->SetHorizontalAlignment(HAlign_Left);
			W->SetVerticalAlignment(VAlign_Top);
			W->SetFillEmptySpace(false); // фикс-размерные слоты, не растягиваем
		}

		InventorySlots.Add(SlotWidget);
	}

	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Created %d inventory slots (WrapBox)"), InventorySlots.Num());

	// Update stats after creating grid
	UpdateInventoryStats();
}

void UInventoryWidget::ClearInventoryGrid()
{
	if (InventoryWrap) InventoryWrap->ClearChildren();
	InventorySlots.Empty();
}

UInventorySlotWidget* UInventoryWidget::GetSlotWidget(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < InventorySlots.Num())
	{
		return InventorySlots[SlotIndex];
	}
	return nullptr;
}

void UInventoryWidget::UpdateSlot(int32 SlotIndex, const FInventoryItemStruct& Item)
{
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget)
	{
		SlotWidget->SetItemData(Item);
	}
}

void UInventoryWidget::ClearSlot(int32 SlotIndex)
{
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget)
	{
		SlotWidget->ClearSlot();
	}
}

void UInventoryWidget::OnSlotClicked(int32 SlotIndex)
{
	// Hide tooltip when slot is clicked
	HideTooltip();
	
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget && SlotWidget->HasItem())
	{
		FInventoryItemStruct Item = SlotWidget->GetItemData();
		OnInventorySlotClicked.Broadcast(SlotIndex, Item);

		UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Slot %d clicked - Item: %s"), SlotIndex, *Item.name);
	}
}

void UInventoryWidget::OnSlotRightClicked(int32 SlotIndex)
{
	// Hide tooltip when slot is right-clicked
	HideTooltip();
	
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget && SlotWidget->HasItem())
	{
		FInventoryItemStruct Item = SlotWidget->GetItemData();
		OnInventorySlotRightClicked.Broadcast(SlotIndex, Item);

		UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Slot %d right-clicked - Item: %s"), SlotIndex, *Item.name);
	}
}

void UInventoryWidget::OnSlotHovered(int32 SlotIndex, bool bIsHovered)
{
	if (bIsHovered)
	{
		HoveredSlotIndex = SlotIndex;
		UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
		if (SlotWidget && SlotWidget->HasItem())
		{
			// Get mouse position for tooltip
			FVector2D MousePosition = FVector2D::ZeroVector;
			if (GetWorld() && GetWorld()->GetFirstPlayerController())
			{
				GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
			}

			ShowTooltip(SlotWidget->GetItemData(), MousePosition);
		}
	}
	else
	{
		if (HoveredSlotIndex == SlotIndex)
		{
			HoveredSlotIndex = -1;
			HideTooltip();
		}
	}
}

void UInventoryWidget::OnInventoryUpdated(const FCharacterInventoryStruct& UpdatedInventory)
{
	UpdateInventoryDisplay(UpdatedInventory);
}

void UInventoryWidget::OnItemAdded(const FInventoryItemStruct& Item, int32 Quantity)
{
	// Visual feedback for item addition could be added here
	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Item added - %s x%d"), *Item.name, Quantity);
}

void UInventoryWidget::OnItemRemoved(const FInventoryItemStruct& Item, int32 Quantity)
{
	// Visual feedback for item removal could be added here
	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Item removed - %s x%d"), *Item.name, Quantity);
}

void UInventoryWidget::ShowTooltip(const FInventoryItemStruct& Item, FVector2D Position)
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetItemData(Item);
		ItemTooltipWidget->UpdateTooltipPosition(Position);
		ItemTooltipWidget->ShowTooltip();
	}
}

void UInventoryWidget::HideTooltip()
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->HideTooltip();
	}
}

int32 UInventoryWidget::GetSlotIndexFromPosition(int32 Row, int32 Column) const
{
	return Row * InventoryColumns + Column;
}

void UInventoryWidget::GetPositionFromSlotIndex(int32 SlotIndex, int32& OutRow, int32& OutColumn) const
{
	OutRow = SlotIndex / InventoryColumns;
	OutColumn = SlotIndex % InventoryColumns;
}

void UInventoryWidget::OnCloseButtonClicked()
{
	SetInventoryVisible(false);
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsInventoryVisible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Hide tooltip when clicking anywhere in the inventory window
		HideTooltip();
		
		bool bShouldStartDrag = false;

		if (DragHandle)
		{
			// Получаемгеометрию DragHandle
			const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
			const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

			// Проверяем, попал ли клик в область DragHandle
			const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
			bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
				LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);

			UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: DragHandle click test - LocalPos: %s, Size: %s, ShouldDrag: %s"),
				*LocalMousePos.ToString(), *DragHandleSize.ToString(), bShouldStartDrag ? TEXT("True") : TEXT("False"));
		}
		else
		{
			// Если нет DragHandle, разрешаем перетаскивание везде
			bShouldStartDrag = true;
			UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: No DragHandle, allowing drag from anywhere"));
		}

		if (bShouldStartDrag)
		{
			const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);

			const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
			const FVector2D MouseVP = Screen / Scale;

			// Важно! Используем текущую позицию виджета, а не геометрию из события
			const FVector2D CurrentPos = CurrentViewportPosition; // Используем отслеживаемую позицию

			DragOffset = MouseVP - CurrentPos;
			bDragging = true;

			UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: Drag calculation - Mouse: %s, Current: %s, Offset: %s"),
				*MouseVP.ToString(), *CurrentPos.ToString(), *DragOffset.ToString());

			if (TSharedPtr<SWidget> Slate = GetCachedWidget())
			{
				return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
			}
			return FReply::Handled();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: Click outside DragHandle area - drag not started"));
		}
	}
	
	// Hide tooltip on right mouse button click as well
	if (bIsInventoryVisible && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		HideTooltip();
	}
	
	return FReply::Unhandled();
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;

		if (TSharedPtr<SWidget> Slate = (DragHandle ? DragHandle->GetCachedWidget() : GetCachedWidget()))
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: Mouse move during drag - Position: %s"),
			*InMouseEvent.GetScreenSpacePosition().ToString());
		UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UInventoryWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPosAbs)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryWidget: No PlayerController found for drag"));
		return;
	}

	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D ViewportSize = FVector2D(W, H) / Scale;

	ForceLayoutPrepass();
	FVector2D Size = GetDesiredSize();
	if (Size.IsZero()) Size = FVector2D(400, 300);

	// курсор -> viewport space
	const FVector2D MouseVP = ScreenCursorPosAbs / Scale;

	// желаемая позиция в viewport space
	FVector2D Pos = MouseVP - DragOffset;

	// клампы
	Pos.X = FMath::Clamp(Pos.X, DragPadding.Left, ViewportSize.X - Size.X - DragPadding.Right);
	Pos.Y = FMath::Clamp(Pos.Y, DragPadding.Top, ViewportSize.Y - Size.Y - DragPadding.Bottom);

	UE_LOG(LogTemp, Warning, TEXT("InventoryWidget: Previous position: %s, New position: %s"),
		*CurrentViewportPosition.ToString(), *Pos.ToString());

	// Update our tracked position
	CurrentViewportPosition = Pos;
	SetPositionInViewport(Pos, false);
}

float UInventoryWidget::GetCurrentInventoryWeight() const
{
	float TotalWeight = 0.0f;
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		TotalWeight += Item.weight * Item.quantity;
	}
	return TotalWeight;
}

int32 UInventoryWidget::GetUsedSlots() const
{
	return CurrentInventory.items.Num();
}

int32 UInventoryWidget::GetFreeSlots() const
{
	return GetTotalSlots() - GetUsedSlots();
}

void UInventoryWidget::UpdateInventoryStats()
{
	// Update weight display
	if (WeightText)
	{
		float CurrentWeight = GetCurrentInventoryWeight();
		FString WeightString = FString::Printf(TEXT("Weight: %.1f/%.1f"), CurrentWeight, MaxInventoryWeight);
		WeightText->SetText(FText::FromString(WeightString));
		
		// Optional: Change color based on weight percentage
		float WeightPercentage = MaxInventoryWeight > 0.0f ? (CurrentWeight / MaxInventoryWeight) : 0.0f;
		if (WeightPercentage >= 1.0f)
		{
			WeightText->SetColorAndOpacity(FLinearColor::Red);
		}
		else if (WeightPercentage >= 0.8f)
		{
			WeightText->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			WeightText->SetColorAndOpacity(FLinearColor::White);
		}
	}

	// Update slots display
	if (SlotsText)
	{
		int32 UsedSlots = GetUsedSlots();
		int32 TotalSlots = GetTotalSlots();
		FString SlotsString = FString::Printf(TEXT("Slots: %d/%d"), UsedSlots, TotalSlots);
		SlotsText->SetText(FText::FromString(SlotsString));
		
		// Optional: Change color based on slots percentage
		float SlotsPercentage = TotalSlots > 0 ? (static_cast<float>(UsedSlots) / static_cast<float>(TotalSlots)) : 0.0f;
		if (SlotsPercentage >= 1.0f)
		{
			SlotsText->SetColorAndOpacity(FLinearColor::Red);
		}
		else if (SlotsPercentage >= 0.8f)
		{
			SlotsText->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			SlotsText->SetColorAndOpacity(FLinearColor::White);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Stats updated - Weight: %.1f/%.1f, Slots: %d/%d"), 
		GetCurrentInventoryWeight(), MaxInventoryWeight, GetUsedSlots(), GetTotalSlots());
}

void UInventoryWidget::SetMaxInventoryWeight(float NewMaxWeight)
{
	MaxInventoryWeight = FMath::Max(0.0f, NewMaxWeight);
	UpdateInventoryStats();
	
	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Max weight set to %.1f"), MaxInventoryWeight);
}