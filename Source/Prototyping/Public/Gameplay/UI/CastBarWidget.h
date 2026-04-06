#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastBarWidget.generated.h"

/**
 * Self-contained cast bar widget.
 *
 * Add this widget to WBP_PlayerInterfaceWidget and name it "CastBarWidget".
 * Inside the widget BP, bind three child widgets:
 *   - CastBar         (UProgressBar) — the fill bar
 *   - CastBarLabel    (UTextBlock)   — skill name text ("Fireball")
 *   - CastBarTimeText (UTextBlock)   — elapsed / total text ("1.4 / 2.0")
 *
 * The widget hides itself when not casting (Collapsed).
 * NativeTick drives the fill; no Blueprint wiring is required.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UCastBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Show the cast bar and begin filling it over CastDuration seconds.
     * @param CastDuration  Total cast time in seconds (must be > 0).
     * @param InSkillName   Skill name displayed on the label.
     */
    UFUNCTION(BlueprintCallable, Category = "CastBar")
    void ShowCastBar(float CastDuration, const FString& InSkillName);

    /** Hide the cast bar immediately (interrupt, finish, or cancel). */
    UFUNCTION(BlueprintCallable, Category = "CastBar")
    void HideCastBar();

    /** Returns true while a cast is in progress. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CastBar")
    bool IsCasting() const { return bIsCasting; }

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CastBar")
    class UProgressBar* CastBar;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CastBar")
    class UTextBlock* CastBarLabel;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CastBar")
    class UTextBlock* CastBarTimeText;

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    bool    bIsCasting   = false;
    float   CastElapsed  = 0.0f;
    float   CastTotal    = 0.0f;
    FString CastSkillName;

    void SetChildVisibility(ESlateVisibility Vis);
};
