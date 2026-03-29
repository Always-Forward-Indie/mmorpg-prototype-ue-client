#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "NotificationAtmosphereWidget.generated.h"

class UTextBlock;

/**
 * NotificationAtmosphereWidget
 *
 * Semi-transparent atmospheric text shown at the screen center for ambient
 * notifications (channel "atmosphere", e.g. pity_hint).
 * No sound, no frame — just flavour text that fades after a few seconds.
 *
 * Blueprint subclass must bind:
 *   Atmo_Text   UTextBlock  — the atmospheric message
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UNotificationAtmosphereWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Notification|Atmosphere")
    void ShowAtmosphere(const FWorldNotificationStruct& Notification);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Atmosphere|Config")
    float DisplayDuration = 3.0f;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|Atmosphere")
    void PlayShowAnimation();
    virtual void PlayShowAnimation_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|Atmosphere")
    void PlayHideAnimation();
    virtual void PlayHideAnimation_Implementation() { SetVisibility(ESlateVisibility::Collapsed); }

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Notification|Atmosphere")
    UTextBlock* Atmo_Text = nullptr;

private:
    FTimerHandle AutoHideTimer;
};
