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
		return FLinearColor::Red; // ������ �������� - �������
	}
	else if (Weight > MediumWeightThreshold)
	{
		return FLinearColor::Yellow; // ������� ��� - �����
	}
	else if (Weight > LightWeightThreshold)
	{
		return FLinearColor(1.0f, 0.8f, 0.4f, 1.0f); // ˸���� �������� - ���������
	}
	
	// ����� ����� �������� �������� ������
	return Col;
}

FText UItemTooltipWidget::FormatWeightText(float Weight) const
{
	if (Weight <= 0.0f)
	{
		return FText::GetEmpty();
	}

	// ��������� ������� ����� ����
	FString WeightString;
	
	// ���� ��� �������� ����� ������ (��� ������� �����)
	if (FMath::IsNearlyEqual(Weight, FMath::RoundToFloat(Weight), 0.001f))
	{
		// ���������� ��� ����� �����
		WeightString = FString::Printf(TEXT("%d"), FMath::RoundToInt(Weight));
	}
	else
	{
		// ���������� � ����� ������� ����� �������, ������� ������ ����
		WeightString = FString::Printf(TEXT("%.2f"), Weight);
		
		// ������� ������ ���� � �����
		while (WeightString.EndsWith(TEXT("0")) && WeightString.Contains(TEXT(".")))
		{
			WeightString = WeightString.LeftChop(1);
		}
		if (WeightString.EndsWith(TEXT(".")))
		{
			WeightString = WeightString.LeftChop(1);
		}
	}
	
	// ��������� ������� ��������� ���� ��������
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
	RarityColors.Add(TEXT("very_rare"), FLinearColor(0.3f, 0.3f, 1.0f, 1.0f));
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

	AttributesBox->ClearChildren();
}

UTextBlock* UItemTooltipWidget::AddAttributeTextWidget(const FString& AttributeName, const FString& AttributeValue)
{
	if (!AttributesBox)
	{
		return nullptr;
	}

	FText FormattedText = FormatAttributeText(AttributeName, AttributeValue);

	if (AttributeTextWidgetClass)
	{
		UUserWidget* RowWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), AttributeTextWidgetClass);
		if (RowWidget)
		{
			UTextBlock* RowText = Cast<UTextBlock>(RowWidget->GetWidgetFromName(TEXT("Row_Text")));
			if (RowText)
				RowText->SetText(FormattedText);
			AttributesBox->AddChild(RowWidget);
			return RowText;
		}
	}

	UTextBlock* AttributeTextWidget = NewObject<UTextBlock>(this);
	if (!AttributeTextWidget)
	{
		return nullptr;
	}

	AttributeTextWidget->SetText(FormattedText);
	AttributeTextWidget->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));

	FSlateFontInfo FontInfo = AttributeTextWidget->GetFont();
	FontInfo.Size = 12;
	AttributeTextWidget->SetFont(FontInfo);

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
