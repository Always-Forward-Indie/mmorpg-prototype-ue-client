#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

UItemTooltipWidget::UItemTooltipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentItem = FInventoryItemStruct();
	TooltipOffset = FVector2D(10.0f, -10.0f);
	FadeInDuration = 0.2f;
	FadeOutDuration = 0.1f;
	bIsVisible = false;
	SetIsFocusable(false);
}

void UItemTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize colors
	InitializeColors();
	// Hide tooltip initially
	SetVisibility(ESlateVisibility::Hidden);
	SetRenderOpacity(0.0f);

	// Subscribe to inventory updates so kill count refreshes live while the tooltip is open
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (UInventoryManager* InvMgr = GI->GetInventoryManager())
		{
			InvMgr->OnInventoryUpdated.AddDynamic(this, &UItemTooltipWidget::OnInventoryUpdated);
		}
	}
}

void UItemTooltipWidget::OnInventoryUpdated(const FCharacterInventoryStruct& UpdatedInventory)
{
	if (CurrentItem.id <= 0 || !bIsVisible) return;

	// Find the matching item in the updated inventory and refresh kill count
	for (const FInventoryItemStruct& Item : UpdatedInventory.items)
	{
		if (Item.id == CurrentItem.id)
		{
			if (Item.killCount != CurrentItem.killCount)
			{
				CurrentItem.killCount = Item.killCount;
				UpdateKillCount();
			}
			break;
		}
	}
}

void UItemTooltipWidget::SetItemData(const FInventoryItemStruct& Item)
{
	CurrentItem = Item;

	// Resolve localised name and description from LocalizationSubsystem (keyed by slug)
	const FString& LookupSlug = CurrentItem.slug.IsEmpty() ? CurrentItem.itemSlug : CurrentItem.slug;
	if (!LookupSlug.IsEmpty())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
			{
				FText LocalName = Loc->GetItemDisplayName(LookupSlug);
				if (!LocalName.IsEmpty())
					CurrentItem.name = LocalName.ToString();

				FText LocalDesc = Loc->GetItemDescription(LookupSlug);
				if (!LocalDesc.IsEmpty())
					CurrentItem.description = LocalDesc.ToString();
			}
		}
	}

	UpdateTooltipContent();
	LoadItemIcon();
}

