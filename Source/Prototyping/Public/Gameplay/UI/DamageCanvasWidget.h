#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "DamageCanvasWidget.generated.h"

/**
 * Separate widget specifically for handling floating combat text and damage effects
 * This widget provides a canvas where damage numbers and effects can be displayed
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UDamageCanvasWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    /** Gets the main damage canvas for floating combat text */
    UFUNCTION(BlueprintCallable, Category = "Damage Canvas")
    UCanvasPanel* GetDamageCanvas() const { return DamageCanvas; }

    /** Initialize the damage canvas */
    UFUNCTION(BlueprintCallable, Category = "Damage Canvas")
    void InitializeDamageCanvas();

protected:
    /** Main canvas panel for damage effects and floating combat text */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Damage Canvas")
    UCanvasPanel* DamageCanvas;

    // Widget overrides
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

public:
    /** Clear all damage effects from the canvas */
    UFUNCTION(BlueprintCallable, Category = "Damage Canvas")
    void ClearAllDamageEffects();

    /** Check if the damage canvas is ready for use */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Damage Canvas")
    bool IsDamageCanvasReady() const { return DamageCanvas != nullptr; }
};