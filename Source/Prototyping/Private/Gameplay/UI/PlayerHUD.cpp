#include "Gameplay/UI/PlayerHUD.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerHUD::NativeConstruct()
{
	Super::NativeConstruct();
	TargetHPPercent     = 1.0f;
	DisplayedHPPercent  = 1.0f;
	HPDamageFlashTimer  = 0.0f;
	HPHealFlashTimer    = 0.0f;
	TargetMPPercent     = 1.0f;
	DisplayedMPPercent  = 1.0f;
	MPDamageFlashTimer  = 0.0f;
	MPHealFlashTimer    = 0.0f;
	CurrentMaxHP  = 1.0f;
	CurrentMaxMana = 1.0f;
	if (HealthBar) HealthBar->SetPercent(1.0f);
	if (ManaBar)   ManaBar->SetPercent(1.0f);
	if (HealthBar) HealthBar->SetFillColorAndOpacity(HPNormalColor);
	if (ManaBar)   ManaBar->SetFillColorAndOpacity(MPNormalColor);
	if (HealthBarTextValue) HealthBarTextValue->SetText(FText::FromString(TEXT("--/--")));
	if (ManaBarTextValue)   ManaBarTextValue->SetText(FText::FromString(TEXT("--/--")));
}

void UPlayerHUD::NativeDestruct()
{
	Super::NativeDestruct();
}

void UPlayerHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TickBar(DisplayedHPPercent, TargetHPPercent, InDeltaTime);
	TickBar(DisplayedMPPercent, TargetMPPercent, InDeltaTime);

	if (HealthBar) HealthBar->SetPercent(DisplayedHPPercent);
	if (ManaBar)   ManaBar->SetPercent(DisplayedMPPercent);

	// HP colour: flash toward damage/heal colour, then lerp back to normal
	if (HPDamageFlashTimer > 0.0f)
	{
		HPDamageFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(HPDamageFlashTimer / DamageFlashDuration, 0.0f, 1.0f);
		if (HealthBar) HealthBar->SetFillColorAndOpacity(LerpColor(HPNormalColor, HPDamageFlashColor, T));
	}
	else if (HPHealFlashTimer > 0.0f)
	{
		HPHealFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(HPHealFlashTimer / HealFlashDuration, 0.0f, 1.0f);
		if (HealthBar) HealthBar->SetFillColorAndOpacity(LerpColor(HPNormalColor, HPHealFlashColor, T));
	}
	else
	{
		if (HealthBar) HealthBar->SetFillColorAndOpacity(HPNormalColor);
	}

	// MP colour flash
	if (MPDamageFlashTimer > 0.0f)
	{
		MPDamageFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(MPDamageFlashTimer / DamageFlashDuration, 0.0f, 1.0f);
		if (ManaBar) ManaBar->SetFillColorAndOpacity(LerpColor(MPNormalColor, MPDamageFlashColor, T));
	}
	else if (MPHealFlashTimer > 0.0f)
	{
		MPHealFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(MPHealFlashTimer / HealFlashDuration, 0.0f, 1.0f);
		if (ManaBar) ManaBar->SetFillColorAndOpacity(LerpColor(MPNormalColor, MPHealFlashColor, T));
	}
	else
	{
		if (ManaBar) ManaBar->SetFillColorAndOpacity(MPNormalColor);
	}
}

void UPlayerHUD::SetHP(float NewHP, float MaxHP)
{
	CurrentMaxHP = FMath::Max(MaxHP, 1.0f);
	const float NewPct = FMath::Clamp(NewHP / CurrentMaxHP, 0.0f, 1.0f);

	if (HealthBarTextValue)
	{
		HealthBarTextValue->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f/%.0f"), NewHP, MaxHP)));
	}

	if (!FMath::IsNearlyEqual(TargetHPPercent, NewPct, 0.001f))
	{
		if (NewPct < TargetHPPercent)
		{
			HPDamageFlashTimer = DamageFlashDuration;
			HPHealFlashTimer   = 0.0f;
		}
		else
		{
			HPHealFlashTimer   = HealFlashDuration;
			HPDamageFlashTimer = 0.0f;
		}
		TargetHPPercent = NewPct;
	}
}

void UPlayerHUD::SetMana(float NewMana, float MaxMana)
{
	CurrentMaxMana = FMath::Max(MaxMana, 1.0f);
	const float NewPct = FMath::Clamp(NewMana / CurrentMaxMana, 0.0f, 1.0f);

	if (ManaBarTextValue)
	{
		ManaBarTextValue->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f/%.0f"), NewMana, MaxMana)));
	}

	if (!FMath::IsNearlyEqual(TargetMPPercent, NewPct, 0.001f))
	{
		if (NewPct < TargetMPPercent)
		{
			MPDamageFlashTimer = DamageFlashDuration;
			MPHealFlashTimer   = 0.0f;
		}
		else
		{
			MPHealFlashTimer   = HealFlashDuration;
			MPDamageFlashTimer = 0.0f;
		}
		TargetMPPercent = NewPct;
	}
}

bool UPlayerHUD::IsHUDReady() const
{
	return HealthBar != nullptr && HealthBarTextValue != nullptr
		&& ManaBar != nullptr && ManaBarTextValue != nullptr;
}

void UPlayerHUD::TickBar(float& DisplayedPct, float TargetPct, float DeltaTime)
{
	if (FMath::IsNearlyEqual(DisplayedPct, TargetPct, 0.001f))
	{
		DisplayedPct = TargetPct;
		return;
	}
	DisplayedPct = FMath::FInterpConstantTo(DisplayedPct, TargetPct, DeltaTime, BarInterpSpeed);
}

FLinearColor UPlayerHUD::LerpColor(const FLinearColor& A, const FLinearColor& B, float T) const
{
	return FLinearColor(
		FMath::Lerp(A.R, B.R, T),
		FMath::Lerp(A.G, B.G, T),
		FMath::Lerp(A.B, B.B, T),
		FMath::Lerp(A.A, B.A, T)
	);
}
