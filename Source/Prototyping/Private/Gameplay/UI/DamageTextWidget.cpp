#include "Gameplay/UI/DamageTextWidget.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"

void UDamageTextWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bIsConstructed = true;

	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::NativeConstruct – DamageText=%s, ShowAnim=%s, HasPendingInit=%s"), 
		*GetNameSafe(DamageText), *GetNameSafe(ShowAnim), bPendingInit ? TEXT("true") : TEXT("false"));

	// CRITICAL: Force widget to be visible IMMEDIATELY after construction
	SetVisibility(ESlateVisibility::Visible);
	SetRenderOpacity(1.0f);
	
	// Make sure the damage text itself is also visible
	if (DamageText)
	{
		DamageText->SetVisibility(ESlateVisibility::Visible);
		DamageText->SetRenderOpacity(1.0f);
	}

	if (bPendingInit)
	{
		bPendingInit = false;
		if (bIsSpecialText)
		{
			InitSpecialText(PendingSpecialText, PendingSpecialColor);
		}
		else
		{
			Init(PendingDamage, PendingCrit, PendingType);
		}
		UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::NativeConstruct - Applied pending initialization"));
	}

	// Force another visibility check after everything is set up
	if (GetVisibility() != ESlateVisibility::Visible)
	{
		UE_LOG(LogTemp, Error, TEXT("DamageTextWidget::NativeConstruct - Widget visibility failed! Current: %d"), (int32)GetVisibility());
		
		// Try different visibility modes
		SetVisibility(ESlateVisibility::HitTestInvisible);
		if (GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// Final visibility check after construction
	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::NativeConstruct - Final visibility: %d, opacity: %f"), 
		(int32)GetVisibility(), GetRenderOpacity());
}

void UDamageTextWidget::SetPendingDamage(float Damage, bool bCrit, EDamageType Type)
{
	PendingDamage = Damage;
	PendingCrit = bCrit;
	PendingType = Type;
	bIsSpecialText = false;

	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::SetPendingDamage - Damage: %f, Crit: %s, Constructed: %s"), 
		Damage, bCrit ? TEXT("true") : TEXT("false"), bIsConstructed ? TEXT("true") : TEXT("false"));

	if (bIsConstructed)
	{
		bPendingInit = false;
		Init(Damage, bCrit, Type);
		UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::SetPendingDamage - Immediate initialization complete"));
	}
	else
	{
		bPendingInit = true;
		UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::SetPendingDamage - Marked for pending initialization"));
	}
}

void UDamageTextWidget::SetPendingSpecialText(const FString& Text, FLinearColor Color)
{
	PendingSpecialText = Text;
	PendingSpecialColor = Color;
	bIsSpecialText = true;

	if (bIsConstructed)
	{
		bPendingInit = false;
		InitSpecialText(Text, Color);
	}
	else
	{
		bPendingInit = true;
	}
}

void UDamageTextWidget::Init(float Damage, bool bCrit, EDamageType Type)
{
	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::Init called - Damage: %f, DamageText valid: %s"), 
		Damage, IsValid(DamageText) ? TEXT("true") : TEXT("false"));

	if (!IsValid(DamageText))
	{
		UE_LOG(LogTemp, Error, TEXT("DamageText is NULL in widget %s (probably failed to bind after minimize)"), *GetName());
		return;
	}

	FString Text = bCrit ? FString::Printf(TEXT("CRIT: %.0f"), Damage) : FString::Printf(TEXT("%.0f"), Damage);
	DamageText->SetText(FText::FromString(Text));

	FLinearColor Color = bCrit ? FLinearColor::Yellow : FLinearColor::White;
	switch (Type)
	{
	case EDamageType::Fire:   Color = FLinearColor::Red; break;
	case EDamageType::Ice:    Color = FLinearColor::Blue; break;
	case EDamageType::Poison: Color = FLinearColor::Green; break;
	default: break;
	}
	DamageText->SetColorAndOpacity(Color);

	// Force text visibility
	DamageText->SetVisibility(ESlateVisibility::Visible);
	DamageText->SetRenderOpacity(1.0f);

	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::Init - Set text: '%s', Color: %s, Text visible: %s"), 
		*Text, *Color.ToString(), DamageText->GetVisibility() == ESlateVisibility::Visible ? TEXT("true") : TEXT("false"));
}

void UDamageTextWidget::InitSpecialText(const FString& Text, FLinearColor Color)
{
	if (!IsValid(DamageText))
	{
		UE_LOG(LogTemp, Error, TEXT("DamageText is NULL in widget %s for special text"), *GetName());
		return;
	}

	DamageText->SetText(FText::FromString(Text));
	DamageText->SetColorAndOpacity(Color);

	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::InitSpecialText - Set text: '%s' with color %s"), 
		*Text, *Color.ToString());
}

void UDamageTextWidget::PlayDamageAnimation()
{
	UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation called - ShowAnim valid: %s, Widget visible: %s"), 
		IsValid(ShowAnim) ? TEXT("true") : TEXT("false"),
		GetVisibility() == ESlateVisibility::Visible ? TEXT("true") : TEXT("false"));

	if (!IsValid(ShowAnim))
	{
		UE_LOG(LogTemp, Error, TEXT("PlayDamageAnimation - ShowAnim is invalid!"));
		return;
	}

	// CRITICAL: Stop any currently playing animation and reset animation state
	if (IsAnimationPlaying(ShowAnim))
	{
		StopAnimation(ShowAnim);
		UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - Stopped previous animation"));
	}

	// Clear any existing timer
	if (GetWorld() && TimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
	}

	// IMPORTANT: Unbind and rebind animation delegate to ensure clean state
	if (ShowAnim && AnimationFinishedDelegate.IsBound())
	{
		UnbindFromAnimationFinished(ShowAnim, AnimationFinishedDelegate);
		AnimationFinishedDelegate.Unbind();
		UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - Unbound previous animation delegate"));
	}

	// Reset animation to the beginning
	SetAnimationCurrentTime(ShowAnim, 0.0f);

	// CRITICAL: Force visibility before playing animation
	if (GetVisibility() != ESlateVisibility::Visible)
	{
		SetVisibility(ESlateVisibility::Visible);
		SetRenderOpacity(1.0f);
		
		// If still not visible, try other modes
		if (GetVisibility() != ESlateVisibility::Visible)
		{
			SetVisibility(ESlateVisibility::HitTestInvisible);
			UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - Used HitTestInvisible fallback"));
		}
	}

	// Make sure DamageText is visible too
	if (DamageText)
	{
		DamageText->SetVisibility(ESlateVisibility::Visible);
		DamageText->SetRenderOpacity(1.0f);
		
		FString DamageTextContent = DamageText->GetText().ToString();
		UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - DamageText visibility set to Visible, text: %s"), *DamageTextContent);
	}

	// Bind animation finished delegate
	AnimationFinishedDelegate.BindDynamic(this, &UDamageTextWidget::OnAnimCompleted);
	BindToAnimationFinished(ShowAnim, AnimationFinishedDelegate);
	UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - Bound animation finished delegate"));

	// Play the animation
	PlayAnimation(ShowAnim);
	
	// Get actual animation duration
	float AnimDuration = ShowAnim ? ShowAnim->GetEndTime() : 1.0f;
	UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation – animation started, duration: %f"), AnimDuration);

	// Set fallback timer with extra safety margin
	if (GetWorld())
	{
		float FallbackTime = FMath::Max(AnimDuration * 1.5f, 2.0f); // At least 1.5x animation duration or 2 seconds
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UDamageTextWidget::OnAnimCompleted, FallbackTime, false);
		UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - Set fallback timer for %f seconds"), FallbackTime);
	}

	// Final check
	UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation - Final widget visibility: %d, opacity: %f"), 
		(int32)GetVisibility(), GetRenderOpacity());
}

void UDamageTextWidget::OnAnimCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::OnAnimCompleted - Animation finished"));

	// Clear timer to prevent double execution
	if (UWorld* World = GetWorld())
	{
		if (TimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
			TimerHandle.Invalidate();
		}
	}

	// Unbind animation delegate
	if (ShowAnim && AnimationFinishedDelegate.IsBound())
	{
		UnbindFromAnimationFinished(ShowAnim, AnimationFinishedDelegate);
		AnimationFinishedDelegate.Unbind();
	}

	if (OwningManager)
	{
		OwningManager->ReturnToPool(this);
	}

	// Reset special text flag for next use
	bIsSpecialText = false;
}

void UDamageTextWidget::SetOwningManager(UFloatingCombatTextManager* Manager)
{
	OwningManager = Manager;
}

void UDamageTextWidget::NativeDestruct()
{
	Super::NativeDestruct();
	bIsConstructed = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}

	if (ShowAnim && AnimationFinishedDelegate.IsBound())
	{
		UnbindFromAnimationFinished(ShowAnim, AnimationFinishedDelegate);
		AnimationFinishedDelegate.Unbind();
	}
}

void UDamageTextWidget::ResetWidgetState()
{
	UE_LOG(LogTemp, Warning, TEXT("DamageTextWidget::ResetWidgetState - Resetting widget state"));

	// Stop any playing animations
	StopAllAnimations();
	
	// Clear any pending timers
	if (GetWorld() && TimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
	}
	
	// Unbind animation delegates to prevent memory leaks
	if (ShowAnim && AnimationFinishedDelegate.IsBound())
	{
		UnbindFromAnimationFinished(ShowAnim, AnimationFinishedDelegate);
		AnimationFinishedDelegate.Unbind();
	}
	
	// Reset animation to the beginning for next use
	if (ShowAnim)
	{
		SetAnimationCurrentTime(ShowAnim, 0.0f);
	}
	
	// Reset widget state variables
	bPendingInit = false;
	bIsSpecialText = false;
	PendingDamage = 0.0f;
	PendingCrit = false;
	PendingSpecialText = "";
	PendingSpecialColor = FLinearColor::White;
	PendingType = EDamageType::Physical;
	
	// Reset visibility
	SetVisibility(ESlateVisibility::Collapsed);
	SetRenderOpacity(1.0f);
	
	if (DamageText)
	{
		DamageText->SetText(FText::GetEmpty());
		DamageText->SetVisibility(ESlateVisibility::Visible);
		DamageText->SetRenderOpacity(1.0f);
	}
}
