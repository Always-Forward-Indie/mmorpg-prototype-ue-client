#include "UI/InventoryWidget.h"
#include "Prototyping.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/ItemActionMenuWidget.h"
#include "UI/DropQuantityPopupWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Components/GridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

#include "Engine/Engine.h"

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InventoryManager = nullptr;
	ItemTooltipWidget = nullptr;
	ItemContextMenuWidget = nullptr;
	DropQuantityPopupWidget = nullptr;
	bIsInventoryVisible = false;
	HoveredSlotIndex = -1;
	CurrentInventory = FCharacterInventoryStruct();
	MaxInventoryWeight = 100.0f;
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();


	// ����� ���� ����� ���� ���������������
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f)); // ���������� �������
	SetAlignmentInViewport(FVector2D(0.f, 0.f));        // �����-���� ��� ������� �����

	// ��������� ������� - ������ ������ ���� � ��������
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		int32 W = 0, H = 0;
		PC->GetViewportSize(W, H);
		const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
		ForceLayoutPrepass();
		const FVector2D Size = GetDesiredSize();

	//	// ������ �� ����� ������
		const float Margin = 100.0f;

	//	// ������� � ������ ������ ����: ������ (W - ������ ������� - ������), ����� (H - ������ ������� - ������)
		CurrentViewportPosition = FVector2D(
			FMath::Max(0.f, VPSizeInit.X - Size.X - Margin),
			FMath::Max(0.f, VPSizeInit.Y - Size.Y - Margin));

		UE_LOG(LogGameplay, Warning, TEXT("InventoryWidget: Setting initial position to bottom-right: %s (Viewport: %dx%d, Size: %s)"),
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

	// Create context menu widget
	if (ItemContextMenuWidgetClass)
	{
		ItemContextMenuWidget = CreateWidget<UItemActionMenuWidget>(this, ItemContextMenuWidgetClass);
		if (ItemContextMenuWidget)
		{
			ItemContextMenuWidget->AddToViewport(1100);
			ItemContextMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			ItemContextMenuWidget->OnActionSelected.AddDynamic(this, &UInventoryWidget::HandleContextAction);
		}
	}

	// Create drop quantity popup widget
	if (DropQuantityPopupWidgetClass)
	{
		DropQuantityPopupWidget = CreateWidget<UDropQuantityPopupWidget>(this, DropQuantityPopupWidgetClass);
		if (DropQuantityPopupWidget)
		{
			DropQuantityPopupWidget->AddToViewport(1200); // topmost
			DropQuantityPopupWidget->SetVisibility(ESlateVisibility::Collapsed);
			DropQuantityPopupWidget->OnDropQuantityConfirmed.AddDynamic(this, &UInventoryWidget::HandleDropConfirmed);
			DropQuantityPopupWidget->OnDropQuantityCancelled.AddDynamic(this, &UInventoryWidget::HandleDropCancelled);
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

	// Clean up context menu
	if (ItemContextMenuWidget)
	{
		ItemContextMenuWidget->RemoveFromParent();
		ItemContextMenuWidget = nullptr;
	}

	// Clean up drop popup
	if (DropQuantityPopupWidget)
	{
		DropQuantityPopupWidget->RemoveFromParent();
		DropQuantityPopupWidget = nullptr;
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

	// ���� ��������� ����� � �������
	if (!bIsInventoryVisible || !ItemTooltipWidget) return;

	// ������ ������� �����?
	const ESlateVisibility Vis = ItemTooltipWidget->GetVisibility();
	const bool bTooltipVisible = (Vis == ESlateVisibility::HitTestInvisible ||
		Vis == ESlateVisibility::SelfHitTestInvisible) &&
		ItemTooltipWidget->GetRenderOpacity() > 0.f;
	if (!bTooltipVisible) return;

	// �������� ������� ���� � ������� ������
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FVector2D Mouse;
		if (PC->GetMousePosition(Mouse.X, Mouse.Y))
		{
			// �� ������ ������ � ����� �������� �������
			ItemTooltipWidget->ForceLayoutPrepass();
			ItemTooltipWidget->UpdateTooltipPosition(Mouse);
		}
	}
}

void UInventoryWidget::InitializeInventory(UInventoryManager* InInventoryManager)
{
	if (!InInventoryManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryWidget: Cannot initialize with null InventoryManager"));
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

	UE_LOG(LogGameplay, Warning, TEXT("InventoryWidget: Successfully initialized with InventoryManager"));
}

