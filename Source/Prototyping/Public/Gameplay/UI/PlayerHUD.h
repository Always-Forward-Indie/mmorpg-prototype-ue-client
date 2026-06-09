#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

	UFUNCTION(BlueprintCallable, Category = "HUD")
	class UProgressBar* GetHealthBar() const { return HealthBar; }
	UFUNCTION(BlueprintCallable, Category = "HUD")
	class UProgressBar* GetManaBar() const { return ManaBar; }

	UFUNCTION(BlueprintCallable, Category = "HUD")
	bool IsHUDReady() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UProgressBar* HealthBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UTextBlock* HealthBarTextValue;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UProgressBar* ManaBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UTextBlock* ManaBarTextValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Animation")
	float BarInterpSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Animation")
	float DamageFlashDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Animation")
	float HealFlashDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPNormalColor = FLinearColor(0.18f, 0.72f, 0.18f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPDamageFlashColor = FLinearColor(0.95f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor HPHealFlashColor = FLinearColor(0.15f, 1.0f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
	FLinearColor MPNormalColor = FLinearColor(0.18f, 0.4f, 0.85f, 1.0f);

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

	void TickBar(float& DisplayedPct, float TargetPct, float DeltaTime);
	FLinearColor LerpColor(const FLinearColor& A, const FLinearColor& B, float T) const;
};
