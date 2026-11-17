#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/PointedProgressWidget.h"
#include "PlayerHUD.generated.h"

UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetHP(float NewHP, float MaxHP);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetMana(float NewMana, float MaxMana);

	// Snap displayed bars to their current targets, skipping interpolation.
	// For discontinuous state changes (revive, travel, first spawn) where
	// lerping from the stale value would look like a glitch.
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SnapBarsToTarget();

	// Optional player portrait (BindWidgetOptional UImage named 'Portrait_Image'
	// in the HUD Blueprint). No-op when the image is not bound.
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetPortrait(class UTexture2D* PortraitTexture);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	class UImage* GetPortraitImage() const { return Portrait_Image; }

	UFUNCTION(BlueprintCallable, Category = "HUD")
	class UPointedProgressWidget* GetHealthBar() const { return HealthBar; }
	UFUNCTION(BlueprintCallable, Category = "HUD")
	class UPointedProgressWidget* GetManaBar() const { return ManaBar; }

	UFUNCTION(BlueprintCallable, Category = "HUD")
	bool IsHUDReady() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UPointedProgressWidget* HealthBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UTextBlock* HealthBarTextValue;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UPointedProgressWidget* ManaBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UTextBlock* ManaBarTextValue;

	// Optional player portrait image (BindWidgetOptional, named 'Portrait_Image').
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "HUD")
	class UImage* Portrait_Image = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Animation")
	float BarInterpSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Animation")
	float DamageFlashDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Animation")
	float HealFlashDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPNormalColor = FLinearColor(FColor(0xC9, 0x60, 0x60));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPBottomNormalColor = FLinearColor(FColor(0x78, 0x2C, 0x37));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPDamageFlashColor = FLinearColor(0.95f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPHealFlashColor = FLinearColor(0.15f, 1.0f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor MPNormalColor = FLinearColor(FColor(0x60, 0x9D, 0xD8));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor MPBottomNormalColor = FLinearColor(FColor(0x28, 0x5A, 0x99));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor MPDamageFlashColor = FLinearColor(0.5f, 0.15f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor MPHealFlashColor = FLinearColor(0.2f, 0.55f, 1.0f, 1.0f);

private:
	float TargetHPPercent = 1.0f;
	float DisplayedHPPercent = 1.0f;
	float HPDamageFlashTimer = 0.0f;
	float HPHealFlashTimer = 0.0f;

	float TargetMPPercent = 1.0f;
	float DisplayedMPPercent = 1.0f;
	float MPDamageFlashTimer = 0.0f;
	float MPHealFlashTimer = 0.0f;

	float CurrentMaxHP = 1.0f;
	float CurrentMaxMana = 1.0f;

	// Heal-flash band [From, To] in fill fractions; driven into the material.
	float HPFlashFrom = 0.0f;
	float HPFlashTo = 0.0f;
	float MPFlashFrom = 0.0f;
	float MPFlashTo = 0.0f;

	void TickBar(float& DisplayedPct, float TargetPct, float DeltaTime);
	FLinearColor LerpColor(const FLinearColor& A, const FLinearColor& B, float T) const;
};
