#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/CanvasPanel.h>
#include "PlayerHUD.generated.h"


/**
 * 
 */
UCLASS()
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

    /** Устанавливает XP */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetXP(float NewXP, float MaxXP);

    /** Устанавливает уровень */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetLevel(int NewLevel);

	//get damage canvas
    UFUNCTION(BlueprintCallable, Category = "HUD")
    UCanvasPanel* GetDamageCanvas() const { return DamageCanvas; }
    /** Получает ссылку на HealthBar */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    class UProgressBar* GetHealthBar() const { return HealthBar; }
    /** Получает ссылку на ManaBar */
    UFUNCTION(BlueprintCallable, Category = "HUD")
	class UProgressBar* GetManaBar() const { return ManaBar; }

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UProgressBar* HealthBar;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
	class UTextBlock* HealthBarTextValue;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UProgressBar* ManaBar;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UTextBlock* ManaBarTextValue;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UProgressBar* XPBar;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UTextBlock* ExpBarTextValue;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    class UTextBlock* LevelText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "HUD")
    UCanvasPanel* DamageCanvas;
};
