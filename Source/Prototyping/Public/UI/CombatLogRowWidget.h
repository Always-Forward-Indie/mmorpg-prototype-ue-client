#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Utils/NetCore/CombatLogRule.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "CombatLogRowWidget.generated.h"

/**
 * Base class for BP-custom combat log rows.
 *
 * Bind any subset (all BindWidgetOptional):
 *   Row_Icon       UImage      — per-type icon (category via GetCategoryId)
 *   Row_Time_Text  UTextBlock  — "[HH:MM:SS]" (empty when timestamps off)
 *   Row_Body_Text  UTextBlock  — the message, tinted with the channel color
 *
 * Assigned via UCombatLogWidget::LogRowWidgetClass. Unassigned = legacy
 * single-UTextBlock line (headless/bot-safe fallback).
 */
UCLASS(Abstract, Blueprintable)
class PROTOTYPING_API UCombatLogRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Fills bound slots; unbound slots are skipped. Color is the resolved
	// channel color (BP override aware).
	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void SetRowData(ECombatLogCategory Category, const FText& Time, const FText& Body, const FLinearColor& Color);

	// Category id for BP-side icon switching (cast to ECombatLogCategory is plain C++).
	UFUNCTION(BlueprintPure, Category = "Combat Log")
	int32 GetCategoryId() const { return static_cast<int32>(RowCategory); }

protected:
	UPROPERTY(meta = (BindWidgetOptional)) UImage*     Row_Icon      = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Row_Time_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Row_Body_Text = nullptr;

	ECombatLogCategory RowCategory = ECombatLogCategory::System;
};
