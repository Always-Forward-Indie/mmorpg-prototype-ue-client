#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Kismet/GameplayStatics.h"

void UFloatingCombatTextManager::Init(UCanvasPanel* InCanvas, APlayerController* InPC, TSubclassOf<UDamageTextWidget> InDamageTextClass)
{
	RootCanvas = InCanvas;
	PlayerController = InPC;
	DamageTextClass = InDamageTextClass;

	UE_LOG(LogTemp, Log, TEXT("FCTManager::Init - DamageTextClass set to %s"), *GetNameSafe(DamageTextClass));
}


void UFloatingCombatTextManager::ShowDamage(const FVector& WorldLocation, float Damage, bool bIsCrit, EDamageType DamageType)
{
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - starting, damage: %f"), Damage);

	// Check if we have valid references BEFORE trying to check their values
	if (!IsValid(PlayerController) || !DamageTextClass || RootCanvas == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Invalid references: PC: %p, Class: %p, Canvas: %p"),
			PlayerController, DamageTextClass.Get(), RootCanvas);
		return;
	}

	FVector2D ScreenPos;

	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,    // контроллер
		WorldLocation,       // мир-координаты
		ScreenPos,           // out: в координатах Canvas
		false                // не относительно игрового окна
	))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to project world location to widget position"));
		return;
	}

	UDamageTextWidget* Widget = GetOrCreateWidget();
	
	// Simplify the complex logging statement to avoid compiler issues
	FString WidgetName = Widget ? Widget->GetName() : TEXT("nullptr");
	bool bIsValid = IsValid(Widget);
	bool bIsInViewport = Widget && Widget->IsInViewport();
	
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - widget received: %s, IsValid: %d, IsInViewport: %d"), 
		*WidgetName, bIsValid ? 1 : 0, bIsInViewport ? 1 : 0);
	
	if (!IsValid(Widget))
	{
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Failed to create or get a valid widget"));
		return;
	}

	FGeometry Geometry = Widget->GetCachedGeometry();
	FVector2D Size = Geometry.GetLocalSize();
	UE_LOG(LogTemp, Warning, TEXT("Widget size: %s"), *Size.ToString());

	UE_LOG(LogTemp, Warning, TEXT("ScreenPos: X=%f Y=%f"), ScreenPos.X, ScreenPos.Y);

	Widget->SetPendingDamage(Damage, bIsCrit, DamageType);

	// Получаем слот; если виджет ещё не был добавлен – добавляем
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot && RootCanvas)
	{
		CanvasSlot = RootCanvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(999);
	}
	if (CanvasSlot)
	{
		// Берём реальный размер (можно GetDesiredSize(), или при необходимости Geometry)
		FVector2D WidgetSize = Widget->GetDesiredSize();
		// Центрируем по точке попадания
		FVector2D AdjustedPos = ScreenPos - WidgetSize * 0.5f;
		CanvasSlot->SetPosition(AdjustedPos);

		UE_LOG(LogTemp, Warning, TEXT("ShowDamage – slot position set to %s"), *AdjustedPos.ToString());
	}

	Widget->PlayDamageAnimation();
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - completed successfully"));
}


UDamageTextWidget* UFloatingCombatTextManager::GetOrCreateWidget()
{
	UE_LOG(LogTemp, Warning, TEXT("GetOrCreateWidget - pool size before cleaning: %d"), WidgetPool.Num());

	// 1) Удаляем все «стёртые» объекты из пула
	WidgetPool.RemoveAll([](const TWeakObjectPtr<UDamageTextWidget>& WPtr)
		{
			return !WPtr.IsValid();
		});

	UE_LOG(LogTemp, Warning, TEXT("GetOrCreateWidget - pool size after RemoveInvalid: %d"), WidgetPool.Num());

	// 2) Ищем первый свободный
	for (auto& WPtr : WidgetPool)
	{
		if (WPtr.IsValid() && !WPtr->IsInViewport())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Reusing widget %s"), *WPtr->GetName());
			return WPtr.Get();
		}
	}

	// 3) Если не нашли — создаём новый
	UE_LOG(LogTemp, Warning, TEXT("  Creating new widget"));
	if (DamageTextClass && PlayerController)
	{
		if (UDamageTextWidget* NewW = CreateWidget<UDamageTextWidget>(PlayerController, DamageTextClass))
		{
			NewW->SetOwningManager(this);
			WidgetPool.Add(NewW);  // TWeakObjectPtr автоматически конструируется из raw ptr
			return NewW;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("GetOrCreateWidget - failed to create widget"));
	return nullptr;
}



void UFloatingCombatTextManager::ReturnToPool(UDamageTextWidget* Widget)
{
	if (IsValid(Widget))
	{
		Widget->RemoveFromParent();
		// Можно сбросить визуально текст, цвет, таймер — если нужно
	}
}

void UFloatingCombatTextManager::BeginDestroy()
{
	if (ReactivateHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(ReactivateHandle);
		ReactivateHandle.Reset();
	}
	Super::BeginDestroy();
}