#include "Gameplay/UI/PlayerHUD.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

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
	if (HealthBar) HealthBar->SetProgress(1.0f);
	if (ManaBar)   ManaBar->SetProgress(1.0f);
	// Shared track palette: #080E16 -> #1D2935, outline #526272 at 45%.
	const FLinearColor TrackTop(FColor(0x08, 0x0E, 0x16));
	const FLinearColor TrackBottom(FColor(0x1D, 0x29, 0x35));
	FLinearColor Border(FColor(0x52, 0x62, 0x72));
	Border.A = 1.0f;
	if (HealthBar)
	{
		HealthBar->SetTrackGradient(TrackTop, TrackBottom);
		HealthBar->SetGhostGradient(
			FLinearColor(FColor(0xE6, 0xA9, 0xA9)),
			FLinearColor(FColor(0x93, 0x42, 0x4B)));
		HealthBar->SetBorderColor(Border);
	}
	if (ManaBar)
	{
		ManaBar->SetTrackGradient(TrackTop, TrackBottom);
		ManaBar->SetGhostGradient(
			FLinearColor(FColor(0xAC, 0xCD, 0xEB)),
			FLinearColor(FColor(0x3E, 0x6E, 0xA6)));
		ManaBar->SetBorderColor(Border);
	}
	if (HealthBar) HealthBar->SetFillGradient(HPNormalColor, HPBottomNormalColor);
	if (ManaBar)   ManaBar->SetFillGradient(MPNormalColor, MPBottomNormalColor);
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

	if (HealthBar) HealthBar->SetProgress(DisplayedHPPercent);
	if (ManaBar)   ManaBar->SetProgress(DisplayedMPPercent);

	// HP colour: flash toward damage/heal colour, then lerp back to normal.
	// The flash drives both gradient stops so the bar keeps its shading.
	if (HPDamageFlashTimer > 0.0f)
	{
		HPDamageFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(HPDamageFlashTimer / DamageFlashDuration, 0.0f, 1.0f);
		if (HealthBar) HealthBar->SetFillGradient(
			LerpColor(HPNormalColor, HPDamageFlashColor, T),
			LerpColor(HPBottomNormalColor, HPDamageFlashColor, T));
	}
	else if (HPHealFlashTimer > 0.0f)
	{
		HPHealFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(HPHealFlashTimer / HealFlashDuration, 0.0f, 1.0f);
		// Glow only the restored chunk; the rest of the fill keeps its gradient.
		if (HealthBar) HealthBar->SetHealFlash(HPFlashFrom, HPFlashTo, HPHealFlashColor, T);
	}
	else
	{
		if (HealthBar) HealthBar->SetFillGradient(HPNormalColor, HPBottomNormalColor);
	}

	// MP colour flash
	if (MPDamageFlashTimer > 0.0f)
	{
		MPDamageFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(MPDamageFlashTimer / DamageFlashDuration, 0.0f, 1.0f);
		if (ManaBar) ManaBar->SetFillGradient(
			LerpColor(MPNormalColor, MPDamageFlashColor, T),
			LerpColor(MPBottomNormalColor, MPDamageFlashColor, T));
	}
	else if (MPHealFlashTimer > 0.0f)
	{
		MPHealFlashTimer -= InDeltaTime;
		const float T = FMath::Clamp(MPHealFlashTimer / HealFlashDuration, 0.0f, 1.0f);
		if (ManaBar) ManaBar->SetHealFlash(MPFlashFrom, MPFlashTo, MPHealFlashColor, T);
	}
	else
	{
		if (ManaBar) ManaBar->SetFillGradient(MPNormalColor, MPBottomNormalColor);
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
			if (HealthBar) HealthBar->SetHealFlash(0.0f, 0.0f, HPHealFlashColor, 0.0f);
		}
		else
		{
			HPHealFlashTimer   = HealFlashDuration;
			HPDamageFlashTimer = 0.0f;
			HPFlashFrom = TargetHPPercent;
			HPFlashTo   = NewPct;
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
			if (ManaBar) ManaBar->SetHealFlash(0.0f, 0.0f, MPHealFlashColor, 0.0f);
		}
		else
		{
			MPHealFlashTimer   = HealFlashDuration;
			MPDamageFlashTimer = 0.0f;
			MPFlashFrom = TargetMPPercent;
			MPFlashTo   = NewPct;
		}
		TargetMPPercent = NewPct;
	}
}

void UPlayerHUD::SnapBarsToTarget()
{
	DisplayedHPPercent = TargetHPPercent;
	DisplayedMPPercent = TargetMPPercent;
	if (HealthBar) { HealthBar->SetProgress(DisplayedHPPercent); HealthBar->SnapGhost(); HealthBar->SetHealFlash(0.0f, 0.0f, HPHealFlashColor, 0.0f); }
	if (ManaBar)   { ManaBar->SetProgress(DisplayedMPPercent);   ManaBar->SnapGhost();   ManaBar->SetHealFlash(0.0f, 0.0f, MPHealFlashColor, 0.0f); }
}

bool UPlayerHUD::IsHUDReady() const
{
	return HealthBar != nullptr && HealthBarTextValue != nullptr
		&& ManaBar != nullptr && ManaBarTextValue != nullptr;
}

void UPlayerHUD::SetPortrait(UTexture2D* PortraitTexture)
{
	if (!Portrait_Image || !PortraitTexture) return;
	Portrait_Image->SetBrushFromTexture(PortraitTexture);
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
