#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUD.generated.h"

/**
 * Main Player HUD widget for displaying health, mana and basic player stats.
 * The cast bar lives in UCastBarWidget, which is a separate optional widget
 * managed by UPlayerInterfaceWidget.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerHUD : public UUserWidget
{
	GENERATED_BODY()
	
public:
    /** Set HP bar and text. */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetHP(float NewHP, float MaxHP);

    /** Set Mana bar and text. */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetMana(float NewMana, float MaxMana);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    class UProgressBar* GetHealthBar() const { return HealthBar; }
    UFUNCTION(BlueprintCallable, Category = "HUD")
	class UProgressBar* GetManaBar() const { return ManaBar; }

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

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
};
