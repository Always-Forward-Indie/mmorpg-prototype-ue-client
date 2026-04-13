#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Data/DataStructs.h"
#include "EmoteItemWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmoteItemClicked,  const FEmoteDefinitionData&, EmoteDef);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmoteItemHovered, const FEmoteDefinitionData&, EmoteDef, bool, bIsHovered);

/**
 * A single emote icon slot in the emote grid.
 *
 * Blueprint bindings (all optional — gracefully skipped when absent):
 *   EmoteIcon_Image     — UImage  (shows the icon from DT_EmoteDefinitions)
 *   EmoteName_Text      — UTextBlock (shows LocalizedName for accessibility / tooltip)
 *   SlotBorder          — UBorder (highlight tint on hover / selection)
 *   SlotButton          — UButton (drives click and hover events)
 *
 * Populate via SetEmoteData(). The widget fires OnEmoteItemClicked when clicked,
 * which the parent EmoteListWidget listens to and forwards to EmoteNetworkHandler.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UEmoteItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Populate the slot with emote data and its visual row from the DataTable. */
    UFUNCTION(BlueprintCallable, Category = "Emote Item Widget")
    void SetEmoteData(const FEmoteDefinitionData& InEmoteDef, UDataTable* InEmoteTable);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emote Item Widget")
    const FEmoteDefinitionData& GetEmoteDef() const { return EmoteDef; }

    /** Fired when the player clicks this slot. Parent widget subscribes. */
    UPROPERTY(BlueprintAssignable, Category = "Emote Item Widget|Events")
    FOnEmoteItemClicked OnEmoteItemClicked;

    /** Fired on hover enter / leave. Useful for tooltip logic. */
    UPROPERTY(BlueprintAssignable, Category = "Emote Item Widget|Events")
    FOnEmoteItemHovered OnEmoteItemHovered;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;

    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

    // ── Blueprint-bound widgets (all optional) ─────────────────────────────

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* EmoteIcon_Image = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* EmoteName_Text = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UBorder* SlotBorder = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UButton* SlotButton = nullptr;

    // ── Style ──────────────────────────────────────────────────────────────

    /** Tint applied to SlotBorder when mouse is hovering this slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote Item Widget|Style")
    FLinearColor HoverTint = FLinearColor(1.f, 1.f, 0.7f, 1.f);

    /** Tint applied to SlotBorder in normal (not-hovered) state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote Item Widget|Style")
    FLinearColor NormalTint = FLinearColor::White;

private:
    UFUNCTION()
    void HandleButtonClicked();

    FEmoteDefinitionData EmoteDef;
};
