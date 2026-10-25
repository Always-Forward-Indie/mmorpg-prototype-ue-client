#include "UI/ItemTooltipWidget.h"
#include "Prototyping.h"
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

void UItemTooltipWidget::UpdateTooltipPosition(FVector2D /*ScreenPosUnused*/)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// 1) ������� ������� � ����������� �������� (UE5)
	const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(PC);

	// 2) �������� ����������� ����������� ������
	SetVisibility(ESlateVisibility::HitTestInvisible); // �� Collapsed
	ForceLayoutPrepass();

	FVector2D Size = GetCachedGeometry().GetLocalSize(); // ������, ��� DesiredSize
	if (Size.X <= 1.f || Size.Y <= 1.f)
		Size = GetDesiredSize();                         // fallback
	if (Size.IsZero())
		Size = FVector2D(300, 200);                      // ���������

	// 3) ������� ��������
	int32 Wpx = 0, Hpx = 0; PC->GetViewportSize(Wpx, Hpx);
	const FVector2D View(Wpx, Hpx);

	const float SafePad = 20.f;
	const FVector2D BaseOffset = TooltipOffset + FVector2D(SafePad, SafePad);

	// 4) ������� ������-�����
	FVector2D Pos = MousePos + BaseOffset;

	// 5) ������ �������������� ��� �������� - ����������� EarlyPad
	const float EarlyPad = SafePad + 50.f; // ��������� � 2.f �� 30.f ��� ����� ������� ������������
	if (Pos.X + Size.X > View.X - EarlyPad)
		Pos.X = MousePos.X - Size.X - FMath::Abs(TooltipOffset.X) - SafePad;

	if (Pos.Y + Size.Y > View.Y - EarlyPad)
		Pos.Y = MousePos.Y - Size.Y - FMath::Abs(TooltipOffset.Y) - SafePad;

	// 6) Ƹ����� ����� (������ ����������� ����)
	Pos.X = FMath::Clamp(Pos.X, 0.f, View.X - Size.X);
	Pos.Y = FMath::Clamp(Pos.Y, 0.f, View.Y - Size.Y);

	// 7) ��������� pivot � ������ �������
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
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotTooltipWidget: GameInstance not found"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventorySlotTooltipWidget: ItemManager not found"));
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

	UE_LOG(LogGameplay, Verbose, TEXT("InventorySlotTooltipWidget: Loading icon for item %s (ID: %d)"), *CurrentItem.name, CurrentItem.itemId);
}

void UItemTooltipWidget::SetIconTexture(UTexture2D* Texture)
{
	if (ItemIcon && Texture)
	{
		ItemIcon->SetBrushFromTexture(Texture);
		ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		//ItemIcon->SetColorAndOpacity(FLinearColor::White);
		ItemIcon->SetRenderOpacity(1.0f);

		UE_LOG(LogGameplay, Verbose, TEXT("Set icon texture %s. Has alpha: %s"), *Texture->GetName(), Texture->HasAlphaChannel() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogGameplay, Verbose, TEXT("InventorySlotTooltipWidget: Set icon texture for item %s"), *CurrentItem.name);
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

	UE_LOG(LogGameplay, Verbose, TEXT("ItemTooltipWidget: Showing tooltip for item %s"), *CurrentItem.name);
}

void UItemTooltipWidget::HideTooltip()
{
	if (!bIsVisible)
	{
		return;
	}

	bIsVisible = false;
	
	ClearKillCountWidget();
	ClearDynamicAttributes();

	SetVisibility(ESlateVisibility::Hidden);
	SetRenderOpacity(0.0f);

	UE_LOG(LogGameplay, Verbose, TEXT("ItemTooltipWidget: Hiding tooltip"));
}

