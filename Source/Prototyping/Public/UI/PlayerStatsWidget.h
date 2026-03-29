#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "Data/DataStructs.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "PlayerStatsWidget.generated.h"

class UPlayerStatsManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerStatsVisibilityChangedDelegate);

/**
 * Draggable character-stats window.
 *
 * Blueprint binds:
 *  Close_Button        – UButton
 *  DragHandle          – UWidget (optional; whole widget is draggable when absent)
 *
 * Level / vitals row (all optional):
 *  Level_Text          – UTextBlock  ("Lv. 7")
 *  HP_Bar              – UProgressBar
 *  HP_Text             – UTextBlock  ("85 / 150")
 *  MP_Bar              – UProgressBar
 *  MP_Text             – UTextBlock  ("40 / 80")
 *  XP_Bar              – UProgressBar
 *  XP_Text             – UTextBlock  ("3400 / 5000")
 *  Weight_Bar          – UProgressBar
 *  Weight_Text         – UTextBlock  ("18.5 / 74.0 kg")
 *
 * Attributes container (all optional):
 *  Attributes_Box      – UVerticalBox  (row widgets added dynamically)
 *
 * Active effects container (all optional):
 *  Effects_Box         – UVerticalBox  (row widgets added dynamically)
 *
 * The designer must also expose:
 *  AttributeRowClass   – TSubclassOf<UUserWidget>  (one text block per attribute row)
 *  EffectRowClass      – TSubclassOf<UUserWidget>  (one text block per effect row)
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerStatsWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    // Bind to a PlayerStatsManager; call once after creation
    UFUNCTION(BlueprintCallable, Category = "Player Stats Widget")
    void BindToStatsManager(UPlayerStatsManager* InStatsManager);

    // Open/close the window
    UFUNCTION(BlueprintCallable, Category = "Player Stats Widget")
    void OpenStats();

    UFUNCTION(BlueprintCallable, Category = "Player Stats Widget")
    void CloseStats();

    UFUNCTION(BlueprintCallable, Category = "Player Stats Widget")
    void ToggleStats();

    // Visibility broadcast (consumed by UIManager for cursor management)
    UPROPERTY(BlueprintAssignable, Category = "Player Stats Widget|Events")
    FOnPlayerStatsVisibilityChangedDelegate OnStatsVisibilityChanged;

protected:
    virtual void NativeConstruct()  override;
    virtual void NativeDestruct()   override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp  (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove      (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // ---- Blueprint-bound widgets (all optional) ----
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* Close_Button = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UWidget* DragHandle = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* Level_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UProgressBar* HP_Bar = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* HP_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UProgressBar* MP_Bar = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* MP_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UProgressBar* XP_Bar = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* XP_Text = nullptr;

    // Shown only when experienceDebt > 0 (red warning text below XP bar)
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* Debt_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UProgressBar* Weight_Bar = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* Weight_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UVerticalBox* Attributes_Box = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UVerticalBox* Effects_Box = nullptr;

    // Optional per-row widget classes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Widget|Classes")
    TSubclassOf<UUserWidget> AttributeRowClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Widget|Classes")
    TSubclassOf<UUserWidget> EffectRowClass;

private:
    // ---- Event handlers ----
    UFUNCTION()
    void HandleCloseButtonClicked();

    UFUNCTION()
    void HandleStatsUpdated(const FPlayerStatsUpdateStruct& NewStats);

    // ---- Refresh helpers ----
    void RefreshAll(const FPlayerStatsUpdateStruct& Stats);
    void RefreshVitals(const FPlayerStatsUpdateStruct& Stats);
    void RefreshAttributes(const FPlayerStatsUpdateStruct& Stats);
    void RefreshEffects(const FPlayerStatsUpdateStruct& Stats);

    // Ticks effect countdown text every second while the window is open
    void TickEffectCountdowns();

    // Start/stop the per-second countdown timer based on whether any timed effects are present
    void UpdateCountdownTimer();

    // ---- Drag support (identical to VendorShopWidget) ----
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UPROPERTY()
    UPlayerStatsManager* StatsManager = nullptr;

    // Last received stats — always kept fresh regardless of window visibility
    FPlayerStatsUpdateStruct CachedStats;

    // Repeating 1-second timer that keeps effect countdown text live
    FTimerHandle EffectCountdownTimer;

    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
    FVector2D DragOffset               = FVector2D::ZeroVector;
    bool      bDragging                = false;
};
