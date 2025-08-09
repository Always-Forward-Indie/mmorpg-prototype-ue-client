#include "Gameplay/UI/DamageTextWidget.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"

void UDamageTextWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bIsConstructed = true;

	if (bPendingInit)
	{
		bPendingInit = false;
		Init(PendingDamage, PendingCrit, PendingType);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("DamageTextWidget::NativeConstruct Ц DamageText=%s, ShowAnim=%s"),
		*GetNameSafe(DamageText),
		*GetNameSafe(ShowAnim));

}

void UDamageTextWidget::SetPendingDamage(float Damage, bool bCrit, EDamageType Type)
{
	PendingDamage = Damage;
	PendingCrit = bCrit;
	PendingType = Type;

	if (bIsConstructed)
	{
		bPendingInit = false;
		Init(Damage, bCrit, Type);
	}
	else
	{
		bPendingInit = true;
	}
}

void UDamageTextWidget::Init(float Damage, bool bCrit, EDamageType Type)
{
	if (!IsValid(DamageText))
	{
		UE_LOG(LogTemp, Error, TEXT("DamageText is NULL in widget %s (probably failed to bind after minimize)"), *GetName());
		return;
	}

	FString Text = bCrit ? FString::Printf(TEXT("CRIT: %.0f"), Damage) : FString::Printf(TEXT("%.0f"), Damage);
	DamageText->SetText(FText::FromString(Text));

	ensureMsgf(DamageText, TEXT("Init: DamageText == nullptr! Binding is broken check Is Variable in UMG."));
	if (!DamageText) return;

	FLinearColor Color = bCrit ? FLinearColor::Yellow : FLinearColor::White;
	switch (Type)
	{
	case EDamageType::Fire:   Color = FLinearColor::Red; break;
	case EDamageType::Ice:    Color = FLinearColor::Blue; break;
	case EDamageType::Poison: Color = FLinearColor::Green; break;
	default: break;
	}
	DamageText->SetColorAndOpacity(Color);
}

void UDamageTextWidget::PlayDamageAnimation()
{
	if (!ShowAnim)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowAnim is missing"));
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	SetRenderOpacity(1.0f);

	// —брос старых анимаций, если что-то зависло
	StopAllAnimations();

	// явный перезапуск с начала
	PlayAnimation(ShowAnim, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f, false);

	UE_LOG(LogTemp, Warning, TEXT("PlayDamageAnimation Ц animation started"));

	// ѕрив€зываем делегат завершени€, если не был прив€зан
	if (!AnimationFinishedDelegate.IsBound())
	{
		AnimationFinishedDelegate.BindDynamic(this, &UDamageTextWidget::OnAnimCompleted);
		BindToAnimationFinished(ShowAnim, AnimationFinishedDelegate);
	}

	// Ќа вс€кий случай Ч fallback таймер
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &UDamageTextWidget::OnAnimCompleted, 1.5f, false);
	}
}


void UDamageTextWidget::OnAnimCompleted()
{
	if (UWorld* World = GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}

	if (OwningManager)
	{
		OwningManager->ReturnToPool(this);
	}
}

void UDamageTextWidget::SetOwningManager(UFloatingCombatTextManager* Manager)
{
	OwningManager = Manager;
}

void UDamageTextWidget::NativeDestruct()
{
	Super::NativeDestruct();

	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);

	if (ShowAnim && AnimationFinishedDelegate.IsBound())
	{
		UnbindFromAnimationFinished(ShowAnim, AnimationFinishedDelegate);
	}
}
