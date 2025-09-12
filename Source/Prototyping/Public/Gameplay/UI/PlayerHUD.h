#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUD.generated.h"

/**
 * Main Player HUD widget for displaying health, mana and basic player stats
 * This widget focuses on core player information display
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerHUD : public UUserWidget
{
	GENERATED_BODY()
	
public:
    /** Устанавливает HP */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetHP(float NewHP, float MaxHP);

    /** Устанавливает Mana */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetMana(float NewMana, float MaxMana);

    /** Получает ссылку на HealthBar */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    class UProgressBar* GetHealthBar() const { return HealthBar; }
    /** Получает ссылку на ManaBar */
    UFUNCTION(BlueprintCallable, Category = "HUD")
	class UProgressBar* GetManaBar() const { return ManaBar; }

    // Check if the HUD is ready for use
    UFUNCTION(BlueprintCallable, Category = "HUD")
    bool IsHUDReady() const;

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UProgressBar* HealthBar;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UTextBlock* HealthBarTextValue;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UProgressBar* ManaBar;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UTextBlock* ManaBarTextValue;

    // Widget overrides
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
};
