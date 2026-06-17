#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UFloatingCombatTextManager::Init(UCanvasPanel* InCanvas, APlayerController* InPC, TSubclassOf<UDamageTextWidget> InDamageTextClass)
{
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::Init called with Canvas: %s, PC: %s, DamageTextClass: %s"), 
		InCanvas ? TEXT("Valid") : TEXT("NULL"),
		InPC ? TEXT("Valid") : TEXT("NULL"), 
		InDamageTextClass ? TEXT("Valid") : TEXT("NULL"));

	RootCanvas = InCanvas;
	PlayerController = InPC;
	DamageTextClass = InDamageTextClass;

	if (InPC)
	{
		CachedLocalPawn = InPC->GetPawn();
	}

	UE_LOG(LogTemp, Log, TEXT("FCTManager::Init - DamageTextClass set to %s"), *GetNameSafe(DamageTextClass));
}

void UFloatingCombatTextManager::ShowDamage(const FVector& WorldLocation, float Damage, bool bIsCrit, EDamageType DamageType, bool bOnLocalPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - starting, damage: %f, location: %s"), Damage, *WorldLocation.ToString());

	if (!IsValid(PlayerController) || !DamageTextClass || RootCanvas == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Invalid references: PC: %s, Class: %s, Canvas: %s"),
			IsValid(PlayerController) ? TEXT("Valid") : TEXT("Invalid"), 
			DamageTextClass ? TEXT("Valid") : TEXT("NULL"), 
			RootCanvas ? TEXT("Valid") : TEXT("NULL"));
		return;
	}

	if (!bOnLocalPlayer)
	{
		APawn* LocalPawn = CachedLocalPawn.Get();
		if (!LocalPawn && PlayerController)
		{
			LocalPawn = PlayerController->GetPawn();
			CachedLocalPawn = LocalPawn;
		}
		if (LocalPawn)
		{
			const float Dist = FVector::Dist(LocalPawn->GetActorLocation(), WorldLocation);
			if (Dist > MaxVisibleDistanceCm) return;
		}
	}

	FVector2D ScreenPos;

	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,    // ����������
		WorldLocation,       // ���-����������
		ScreenPos,           // out: � ����������� Canvas
		false                // �� ������������ �������� ����
	))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to project world location to widget position"));
		return;
	}

	// Spread numbers so concurrent hits don't overlap.
	// Random horizontal offset and slight vertical variation.
	ScreenPos.X += FMath::FRandRange(-50.0f, 50.0f);
	ScreenPos.Y += FMath::FRandRange(-15.0f, 5.0f);

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

	// Check if widget has valid bindings
	if (!Widget->HasValidBindings())
	{
		UE_LOG(LogTemp, Error, TEXT("FCTManager::ShowDamage - Widget has invalid bindings (DamageText or ShowAnim missing)"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ScreenPos: X=%f Y=%f"), ScreenPos.X, ScreenPos.Y);

	// Set pending data first, before adding to canvas
	Widget->SetPendingDamage(Damage, bIsCrit, DamageType, bOnLocalPlayer);

	// Get or create canvas slot
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot && RootCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Adding widget to canvas"));
		CanvasSlot = RootCanvas->AddChildToCanvas(Widget);
		if (CanvasSlot)
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetZOrder(999);
			UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Successfully added widget to canvas"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("FCTManager::ShowDamage - Failed to create canvas slot"));
			return;
		}
	}
	
	if (CanvasSlot)
	{
		// Get the widget's desired size
		FVector2D WidgetSize = Widget->GetDesiredSize();
		if (WidgetSize.IsZero())
		{
			// Use a default size if GetDesiredSize returns zero
			WidgetSize = FVector2D(100.0f, 50.0f);
			UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Using default widget size: %s"), *WidgetSize.ToString());
		}
		
		// Center the widget on the hit point
		FVector2D AdjustedPos = ScreenPos - WidgetSize * 0.5f;
		CanvasSlot->SetPosition(AdjustedPos);

		UE_LOG(LogTemp, Warning, TEXT("ShowDamage � slot position set to X=%f Y=%f, widget size: %s"), 
			AdjustedPos.X, AdjustedPos.Y, *WidgetSize.ToString());
	}

	// CRITICAL FIX: Ensure widget visibility with multiple fallback methods
	// First, try the standard visibility
	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->SetRenderOpacity(1.0f);

	// If the widget is still not visible, force it using different approaches
	if (Widget->GetVisibility() != ESlateVisibility::Visible)
	{
		UE_LOG(LogTemp, Error, TEXT("FCTManager::ShowDamage - Primary visibility failed! Trying fallbacks..."));
		
		// Try HitTestInvisible - sometimes this works when Visible doesn't
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		
		// If still not working, try SelfHitTestInvisible
		if (Widget->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		
		// Force slate widget refresh if available
		if (Widget->GetCachedWidget().IsValid())
		{
			Widget->GetCachedWidget()->Invalidate(EInvalidateWidgetReason::Layout);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - Widget visibility set to: %d, opacity: %f"), 
		(int32)Widget->GetVisibility(), Widget->GetRenderOpacity());

	// Play animation after widget is properly set up
	Widget->PlayDamageAnimation();
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowDamage - completed successfully"));
}

void UFloatingCombatTextManager::ShowMissText(const FVector& WorldLocation)
{
	UE_LOG(LogTemp, Log, TEXT("FCTManager::ShowMissText - showing MISSED at location %s"), *WorldLocation.ToString());
	ShowSpecialText(WorldLocation, TEXT("MISSED"), FLinearColor::Gray);
}

void UFloatingCombatTextManager::ShowBlockedText(const FVector& WorldLocation)
{
	UE_LOG(LogTemp, Log, TEXT("FCTManager::ShowBlockedText - showing BLOCKED at location %s"), *WorldLocation.ToString());
	ShowSpecialText(WorldLocation, TEXT("BLOCKED"), FLinearColor::Blue);
}

void UFloatingCombatTextManager::ShowSpecialText(const FVector& WorldLocation, const FString& Text, FLinearColor Color)
{
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowSpecialText - starting, text: %s"), *Text);

	if (!IsValid(PlayerController) || !DamageTextClass || RootCanvas == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowSpecialText - Invalid references: PC: %p, Class: %p, Canvas: %p"),
			PlayerController, DamageTextClass.Get(), RootCanvas);
		return;
	}

	{
		APawn* LocalPawn = CachedLocalPawn.Get();
		if (!LocalPawn && PlayerController)
		{
			LocalPawn = PlayerController->GetPawn();
			CachedLocalPawn = LocalPawn;
		}
		if (LocalPawn)
		{
			const float Dist = FVector::Dist(LocalPawn->GetActorLocation(), WorldLocation);
			if (Dist > MaxVisibleDistanceCm) return;
		}
	}

	FVector2D ScreenPos;

	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		WorldLocation,
		ScreenPos,
		false
	))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to project world location to widget position for special text"));
		return;
	}

	UDamageTextWidget* Widget = GetOrCreateWidget();
	
	if (!IsValid(Widget))
	{
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowSpecialText - Failed to create or get a valid widget"));
		return;
	}

	// Set up the widget for special text display
	// Use 0 damage to indicate this is special text, not a damage number
	Widget->SetPendingSpecialText(Text, Color);

	// Get or create canvas slot
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot && RootCanvas)
	{
		CanvasSlot = RootCanvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetZOrder(999);
	}
	
	if (CanvasSlot)
	{
		FVector2D WidgetSize = Widget->GetDesiredSize();
		if (WidgetSize.IsZero())
		{
			WidgetSize = FVector2D(100.0f, 50.0f);
		}
		FVector2D AdjustedPos = ScreenPos - WidgetSize * 0.5f;
		CanvasSlot->SetPosition(AdjustedPos);

		UE_LOG(LogTemp, Warning, TEXT("ShowSpecialText � slot position set to %s"), *AdjustedPos.ToString());
	}

	// Make sure the widget is visible
	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->SetRenderOpacity(1.0f);

	Widget->PlayDamageAnimation();
	UE_LOG(LogTemp, Warning, TEXT("FCTManager::ShowSpecialText - completed successfully"));
}

UDamageTextWidget* UFloatingCombatTextManager::GetOrCreateWidget()
{
	UE_LOG(LogTemp, Warning, TEXT("GetOrCreateWidget - pool size before cleaning: %d"), WidgetPool.Num());

	// 1) ������� ��� ������� ������� �� ����
	WidgetPool.RemoveAll([](const TWeakObjectPtr<UDamageTextWidget>& WPtr)
		{
			return !WPtr.IsValid();
		});

	UE_LOG(LogTemp, Warning, TEXT("GetOrCreateWidget - pool size after RemoveInvalid: %d"), WidgetPool.Num());

	// 2) ���� ������ ��������� ������ (�� ����������� � viewport � �� ������������� ��������)
	for (auto& WPtr : WidgetPool)
	{
		if (WPtr.IsValid())
		{
			UDamageTextWidget* Widget = WPtr.Get();
			bool bInViewport = Widget->IsInViewport();
			bool bPlayingAnim = Widget->ShowAnim ? Widget->IsAnimationPlaying(Widget->ShowAnim) : false;
			
			UE_LOG(LogTemp, Warning, TEXT("  Checking widget %s - InViewport: %s, PlayingAnim: %s"), 
				*Widget->GetName(), bInViewport ? TEXT("true") : TEXT("false"), bPlayingAnim ? TEXT("true") : TEXT("false"));
			
			// ������ �������� ���� �� �� � viewport � �� ����������� ��������
			if (!bInViewport && !bPlayingAnim)
			{
				UE_LOG(LogTemp, Warning, TEXT("  Reusing widget %s"), *Widget->GetName());
				
				// Ensure the widget is properly reset and ready to use
				Widget->ResetWidgetState();
				Widget->SetVisibility(ESlateVisibility::Collapsed);
				Widget->SetRenderOpacity(1.0f);
				
				return Widget;
			}
		}
	}

	// 3) ���� �� ����� � ������ �����
	UE_LOG(LogTemp, Warning, TEXT("  Creating new widget"));
	if (DamageTextClass && PlayerController)
	{
		if (UDamageTextWidget* NewW = CreateWidget<UDamageTextWidget>(PlayerController, DamageTextClass))
		{
			NewW->SetOwningManager(this);
			WidgetPool.Add(NewW);  // TWeakObjectPtr ������������� �������������� �� raw ptr
			UE_LOG(LogTemp, Warning, TEXT("  Created new widget %s"), *NewW->GetName());
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
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ReturnToPool - Returning widget %s to pool"), *Widget->GetName());
		
		// Stop any playing animations first
		if (Widget->ShowAnim && Widget->IsAnimationPlaying(Widget->ShowAnim))
		{
			Widget->StopAnimation(Widget->ShowAnim);
		}
		
		// Use the widget's own cleanup function
		Widget->ResetWidgetState();
		
		// Remove from parent
		Widget->RemoveFromParent();
		
		UE_LOG(LogTemp, Warning, TEXT("FCTManager::ReturnToPool - Widget %s returned to pool"), *Widget->GetName());
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

UCanvasPanel* UFloatingCombatTextManager::GetRootCanvas() const
{
	return RootCanvas;
}

APlayerController* UFloatingCombatTextManager::GetPlayerController() const
{
	return PlayerController;
}

TSubclassOf<UDamageTextWidget> UFloatingCombatTextManager::GetDamageTextClass() const
{
	return DamageTextClass;
}