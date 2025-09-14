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
}

void UItemTooltipWidget::SetItemData(const FInventoryItemStruct& Item)
{
	CurrentItem = Item;
	UpdateTooltipContent();
	LoadItemIcon();
}

void UItemTooltipWidget::UpdateTooltipPosition(FVector2D /*ScreenPosUnused*/)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// 1) Позиция курсора в координатах вьюпорта (UE5)
	const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(PC);

	// 2) Получаем максимально достоверный размер
	SetVisibility(ESlateVisibility::HitTestInvisible); // не Collapsed
	ForceLayoutPrepass();

	FVector2D Size = GetCachedGeometry().GetLocalSize(); // точнее, чем DesiredSize
	if (Size.X <= 1.f || Size.Y <= 1.f)
		Size = GetDesiredSize();                         // fallback
	if (Size.IsZero())
		Size = FVector2D(300, 200);                      // страховка

	// 3) Границы вьюпорта
	int32 Wpx = 0, Hpx = 0; PC->GetViewportSize(Wpx, Hpx);
	const FVector2D View(Wpx, Hpx);

	const float SafePad = 20.f;
	const FVector2D BaseOffset = TooltipOffset + FVector2D(SafePad, SafePad);

	// 4) Пробуем справа-внизу
	FVector2D Pos = MousePos + BaseOffset;

	// 5) Раннее зеркалирование БЕЗ урезания - увеличиваем EarlyPad
	const float EarlyPad = SafePad + 50.f; // Увеличили с 2.f до 30.f для более раннего срабатывания
	if (Pos.X + Size.X > View.X - EarlyPad)
		Pos.X = MousePos.X - Size.X - FMath::Abs(TooltipOffset.X) - SafePad;

	if (Pos.Y + Size.Y > View.Y - EarlyPad)
		Pos.Y = MousePos.Y - Size.Y - FMath::Abs(TooltipOffset.Y) - SafePad;

	// 6) Жёсткий кламп (теперь срабатывает реже)
	Pos.X = FMath::Clamp(Pos.X, 0.f, View.X - Size.X);
	Pos.Y = FMath::Clamp(Pos.Y, 0.f, View.Y - Size.Y);

	// 7) Фиксируем pivot и ставим позицию
	SetAlignmentInViewport(FVector2D(0, 0));
	SetPositionInViewport(Pos, /*bRemoveDPIScale=*/false);
}

void UItemTooltipWidget::LoadItemIcon()
{
	if (!ItemIcon)
	{
		return;
	}

	// Get ItemManager from game instance
	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InventorySlotTooltipWidget: GameInstance not found"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogTemp, Error, TEXT("InventorySlotTooltipWidget: ItemManager not found"));
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
			// Load asynchronously
			TWeakObjectPtr<UItemTooltipWidget> WeakThis = this;

			AsyncLoad(IconSoftPtr.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, IconSoftPtr]()
					{
						if (WeakThis.IsValid())
						{
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

	// Show icon
	ItemIcon->SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogTemp, Warning, TEXT("InventorySlotTooltipWidget: Loading icon for item %s (ID: %d)"), *CurrentItem.name, CurrentItem.itemId);
}

void UItemTooltipWidget::SetIconTexture(UTexture2D* Texture)
{
	if (ItemIcon && Texture)
	{
		ItemIcon->SetBrushFromTexture(Texture);
		ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		//ItemIcon->SetColorAndOpacity(FLinearColor::White);
		ItemIcon->SetRenderOpacity(1.0f);

		UE_LOG(LogTemp, Log, TEXT("Set icon texture %s. Has alpha: %s"), *Texture->GetName(), Texture->HasAlphaChannel() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Warning, TEXT("InventorySlotTooltipWidget: Set icon texture for item %s"), *CurrentItem.name);
	}
}

void UItemTooltipWidget::SetDefaultIcon()
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
void UItemTooltipWidget::AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback)
{
	if (!Path.IsValid())
	{
		if (Callback.IsBound()) { Callback.Execute(); }
		return;
	}

	if (UObject* Existing = Path.ResolveObject())
	{
		if (Callback.IsBound()) { Callback.Execute(); }
		return;
	}

	if (UAssetManager* AM = UAssetManager::GetIfInitialized())
	{
		AM->GetStreamableManager().RequestAsyncLoad(Path, MoveTemp(Callback));
	}
	else
	{
		static FStreamableManager StaticSM;
		StaticSM.RequestAsyncLoad(Path, MoveTemp(Callback));
	}
}


void UItemTooltipWidget::ShowTooltip()
{
	if (bIsVisible)
	{
		return;
	}

	bIsVisible = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	ForceLayoutPrepass();

	// Fade in animation
	//UWidgetBlueprintLibrary::SetFocusToGameViewport();
	
	// Simple fade in (you can replace this with a proper animation blueprint)
	SetRenderOpacity(0.0f);
	
	// TODO: Implement proper fade-in animation
	// For now, just set to visible
	SetRenderOpacity(1.0f);

	UE_LOG(LogTemp, Verbose, TEXT("ItemTooltipWidget: Showing tooltip for item %s"), *CurrentItem.name);
}

void UItemTooltipWidget::HideTooltip()
{
	if (!bIsVisible)
	{
		return;
	}

	bIsVisible = false;
	
	// TODO: Implement proper fade-out animation
	// For now, just hide immediately
	SetVisibility(ESlateVisibility::Hidden);
	SetRenderOpacity(0.0f);

	UE_LOG(LogTemp, Verbose, TEXT("ItemTooltipWidget: Hiding tooltip"));
}

void UItemTooltipWidget::UpdateTooltipContent()
{
	UpdateItemHeader();
	UpdateItemDescription();
	UpdateItemTypeAndLevel();
	UpdateItemWeight();
	UpdateItemAttributes();
}

void UItemTooltipWidget::UpdateItemHeader()
{
	// Update item name
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::FromString(CurrentItem.name));
		//ItemNameText->SetColorAndOpacity(GetRarityColor(CurrentItem.rarity));
	}

	// Update rarity text
	if (ItemRarityText)
	{
		FString RarityText = CurrentItem.rarity.IsEmpty() ? TEXT("Common") : CurrentItem.rarity;
		// Capitalize first letter
		if (!RarityText.IsEmpty())
		{
			RarityText = RarityText.Left(1).ToUpper() + RarityText.Mid(1).ToLower();
		}
		ItemRarityText->SetText(FText::FromString(RarityText));
		ItemRarityText->SetColorAndOpacity(GetRarityColor(CurrentItem.rarity));
	}

	// Update item icon (placeholder)
	if (ItemIcon)
	{
		// TODO: Load actual item icon based on item data
		ItemIcon->SetVisibility(ESlateVisibility::Visible);
	}
}

void UItemTooltipWidget::UpdateItemDescription()
{
	if (ItemDescriptionText)
	{
		FString Description = CurrentItem.description;
		if (Description.IsEmpty())
		{
			Description = TEXT("No description available.");
		}
		
		ItemDescriptionText->SetText(FText::FromString(Description));
	}
}

void UItemTooltipWidget::UpdateItemAttributes()
{
	if (!AttributesBox)
	{
		return;
	}

	// Clear existing attribute widgets
	ClearDynamicAttributes();

	// Add attributes
	for (const auto& AttributePair : CurrentItem.attributes)
	{
		if (!AttributePair.Key.IsEmpty() && !AttributePair.Value.IsEmpty())
		{
			AddAttributeTextWidget(AttributePair.Key, AttributePair.Value);
		}
	}

	if(CurrentItem.attributes.Num() == 0)
	{
		AttributesBox->SetVisibility(ESlateVisibility::Collapsed);
		Separator3->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Show quantity if greater than 1
	//if (QuantityText && CurrentItem.quantity > 1)
	//{
	//	QuantityText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentItem.quantity)));
	//	QuantityText->SetVisibility(ESlateVisibility::Visible);
	//}
	//else if (QuantityText)
	//{
	//	QuantityText->SetVisibility(ESlateVisibility::Collapsed);
	//	Separator4->SetVisibility(ESlateVisibility::Collapsed);
	//}
}

void UItemTooltipWidget::UpdateItemTypeAndLevel()
{
	// Update item type
	if (ItemTypeText)
	{
		FString TypeText = CurrentItem.type.IsEmpty() ? TEXT("Unknown") : CurrentItem.type;
		// Capitalize first letter
		if (!TypeText.IsEmpty())
		{
			TypeText = TypeText.Left(1).ToUpper() + TypeText.Mid(1).ToLower();
		}
		ItemTypeText->SetText(FText::FromString(TypeText));
		ItemTypeText->SetColorAndOpacity(GetTypeColor(CurrentItem.type));
	}

	// Update item level
	if (ItemLevelText)
	{
		if (CurrentItem.level_requirement >= 1)
		{
			ItemLevelText->SetText(FText::FromString(FString::Printf(TEXT("Required Level: %d"), CurrentItem.level_requirement)));
			ItemLevelText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ItemLevelText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UItemTooltipWidget::UpdateItemWeight()
{
	// Update item weight
	if (WeightText)
	{
		if (CurrentItem.weight > 0.0f)
		{
			// Используем централизованную функцию форматирования
			FText WeightText_FormattedText = FormatWeightText(CurrentItem.weight);
			
			WeightText->SetText(WeightText_FormattedText);
			WeightText->SetVisibility(ESlateVisibility::Visible);
			
			// Добавляем цветовое кодирование веса
			WeightText->SetColorAndOpacity(GetWeightColor(CurrentItem.weight));
		}
		else
		{
			// Скрываем виджет веса, если вес 0 или отрицательный
			WeightText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FLinearColor UItemTooltipWidget::GetRarityColor(const FString& Rarity) const
{
	if (const FLinearColor* Color = RarityColors.Find(Rarity.ToLower()))
	{
		return *Color;
	}
	
	return FLinearColor::White; // Default color
}

FLinearColor UItemTooltipWidget::GetTypeColor(const FString& Type) const
{
	if (const FLinearColor* Color = TypeColors.Find(Type.ToLower()))
	{
		return *Color;
	}
	
	return FLinearColor(0.7f, 0.7f, 0.7f, 1.0f); // Default gray
}

FLinearColor UItemTooltipWidget::GetWeightColor(float Weight) const
{
	//FColor SRGB = FColor::FromHex(TEXT("CBBDA1FF"));
	FLinearColor Col = FLinearColor(0.796079f, 0.741176f, 0.631373f, 1.0f);

	if (!bShowWeightColorCoding)
	{
		return Col; // Default light gray
	}

	if (Weight > HeavyWeightThreshold)
	{
		return FLinearColor::Red; // Тяжёлые предметы - красным
	}
	else if (Weight > MediumWeightThreshold)
	{
		return FLinearColor::Yellow; // Средний вес - жёлтым
	}
	else if (Weight > LightWeightThreshold)
	{
		return FLinearColor(1.0f, 0.8f, 0.4f, 1.0f); // Лёгкие предметы - оранжевым
	}
	
	// Очень лёгкие предметы остаются белыми
	return Col;
}

FText UItemTooltipWidget::FormatWeightText(float Weight) const
{
	if (Weight <= 0.0f)
	{
		return FText::GetEmpty();
	}

	// Формируем базовый текст веса
	FString WeightString;
	
	// Если вес является целым числом (без дробной части)
	if (FMath::IsNearlyEqual(Weight, FMath::RoundToFloat(Weight), 0.001f))
	{
		// Показываем как целое число
		WeightString = FString::Printf(TEXT("%d"), FMath::RoundToInt(Weight));
	}
	else
	{
		// Показываем с двумя знаками после запятой, убираем лишние нули
		WeightString = FString::Printf(TEXT("%.2f"), Weight);
		
		// Убираем лишние нули в конце
		while (WeightString.EndsWith(TEXT("0")) && WeightString.Contains(TEXT(".")))
		{
			WeightString = WeightString.LeftChop(1);
		}
		if (WeightString.EndsWith(TEXT(".")))
		{
			WeightString = WeightString.LeftChop(1);
		}
	}
	
	// Добавляем единицы измерения если включено
	if (bShowWeightUnit && !WeightUnit.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("Weight: %s %s"), *WeightString, *WeightUnit));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("Weight: %s"), *WeightString));
	}
}

FText UItemTooltipWidget::FormatAttributeText(const FString& AttributeName, const FString& AttributeValue) const
{
	FString FormattedName = FormatAttributeName(AttributeName);
	return FText::FromString(FString::Printf(TEXT("%s: %s"), *FormattedName, *AttributeValue));
}

void UItemTooltipWidget::InitializeColors()
{
	// Initialize rarity colors
	RarityColors.Empty();
	RarityColors.Add(TEXT("common"), FLinearColor::White);
	RarityColors.Add(TEXT("uncommon"), FLinearColor::Green);
	RarityColors.Add(TEXT("rare"), FLinearColor::Blue);
	RarityColors.Add(TEXT("epic"), FLinearColor(0.5f, 0.0f, 1.0f, 1.0f)); // Purple
	RarityColors.Add(TEXT("legendary"), FLinearColor(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
	RarityColors.Add(TEXT("mythic"), FLinearColor::Red);

	// Initialize type colors
	TypeColors.Empty();
	TypeColors.Add(TEXT("weapon"), FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)); // Red
	TypeColors.Add(TEXT("armor"), FLinearColor(0.2f, 0.5f, 1.0f, 1.0f)); // Blue
	TypeColors.Add(TEXT("consumable"), FLinearColor(0.2f, 1.0f, 0.2f, 1.0f)); // Green
	TypeColors.Add(TEXT("quest"), FLinearColor(1.0f, 1.0f, 0.2f, 1.0f)); // Yellow
	TypeColors.Add(TEXT("tool"), FLinearColor(0.8f, 0.4f, 0.2f, 1.0f)); // Brown
	TypeColors.Add(TEXT("resource"), FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
}

void UItemTooltipWidget::ClearDynamicAttributes()
{
	if (!AttributesBox)
	{
		return;
	}

	// Remove dynamically created attribute text widgets
	// Note: This is a simplified approach. In a more complex implementation,
	// you might want to track created widgets separately.
	
	TArray<UWidget*> ChildrenToRemove;
	
	for (int32 i = 0; i < AttributesBox->GetChildrenCount(); i++)
	{
		UWidget* Child = AttributesBox->GetChildAt(i);
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Child))
		{
			// Check if this is a dynamically created attribute widget
			// (you could use tags or other identification methods)
			ChildrenToRemove.Add(Child);
		}
	}

	for (UWidget* Child : ChildrenToRemove)
	{
		AttributesBox->RemoveChild(Child);
	}
}

UTextBlock* UItemTooltipWidget::AddAttributeTextWidget(const FString& AttributeName, const FString& AttributeValue)
{
	if (!AttributesBox)
	{
		return nullptr;
	}

	// Create new text widget using the widget tree
	UTextBlock* AttributeTextWidget = NewObject<UTextBlock>(this);
	if (!AttributeTextWidget)
	{
		return nullptr;
	}

	// Set text content
	AttributeTextWidget->SetText(FormatAttributeText(AttributeName, AttributeValue));
	
	// Style the text (you can customize this)
	AttributeTextWidget->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));

	//set font size
	FSlateFontInfo FontInfo = AttributeTextWidget->GetFont();
	FontInfo.Size = 12; // любой размер в pt
	AttributeTextWidget->SetFont(FontInfo);

	// Add to attributes box
	AttributesBox->AddChild(AttributeTextWidget);

	return AttributeTextWidget;
}

FString UItemTooltipWidget::FormatAttributeName(const FString& AttributeName) const
{
	FString FormattedName = AttributeName;
	
	// Replace underscores with spaces
	FormattedName = FormattedName.Replace(TEXT("_"), TEXT(" "));
	
	// Capitalize each word
	TArray<FString> Words;
	FormattedName.ParseIntoArray(Words, TEXT(" "));
	
	FormattedName.Empty();
	for (int32 i = 0; i < Words.Num(); i++)
	{
		if (!Words[i].IsEmpty())
		{
			FString Word = Words[i].ToLower();
			Word = Word.Left(1).ToUpper() + Word.Mid(1);
			
			if (i > 0)
			{
				FormattedName += TEXT(" ");
			}
			FormattedName += Word;
		}
	}
	
	return FormattedName;
}