#include "UI/VendorTooltipWidget.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/AssetManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// ---------------------------------------------------------------------------
// NativeConstruct
// ---------------------------------------------------------------------------

void UVendorTooltipWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Default rarity colors (can be overridden in Blueprint)
    if (RarityColors.Num() == 0)
    {
        RarityColors.Add(TEXT("common"),    FLinearColor(0.8f, 0.8f, 0.8f));
        RarityColors.Add(TEXT("uncommon"),  FLinearColor(0.13f, 0.68f, 0.13f));
        RarityColors.Add(TEXT("rare"),      FLinearColor(0.0f, 0.44f, 0.87f));
        RarityColors.Add(TEXT("epic"),      FLinearColor(0.64f, 0.21f, 0.93f));
        RarityColors.Add(TEXT("legendary"), FLinearColor(1.0f, 0.5f, 0.0f));
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

