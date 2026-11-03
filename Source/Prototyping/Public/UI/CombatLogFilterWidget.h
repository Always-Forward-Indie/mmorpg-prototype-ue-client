#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Utils/NetCore/CombatLogRule.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "CombatLogFilterWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatLogFilterToggled);

/**
 * Base class for BP-custom combat log channel filters.
 *
 * Bind any subset (all BindWidgetOptional):
 *   FilterCheckBox  UCheckBox   — toggles the channel (unbound = always on)
 *   FilterLabel_Text UTextBlock — channel label, tinted with channel color
 *   FilterIcon      UImage      — optional channel icon
 *
 * Assigned via UCombatLogWidget::LogFilterWidgetClass. Unassigned = legacy
 * checkbox+label cell (headless/bot-safe fallback).
 */
UCLASS(Abstract, Blueprintable)
class PROTOTYPING_API UCombatLogFilterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Fills bound slots; binds the checkbox to the toggle broadcast.
	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void Setup(ECombatLogCategory Category, const FText& Label, const FLinearColor& Color, bool bChecked);

	UFUNCTION(BlueprintPure, Category = "Combat Log")
	bool IsChannelChecked() const;

	UFUNCTION(BlueprintPure, Category = "Combat Log")
	UCheckBox* GetFilterCheckBox() const { return FilterCheckBox; }

	UFUNCTION(BlueprintPure, Category = "Combat Log")
	int32 GetCategoryId() const { return static_cast<int32>(Channel); }

	// C++ side (UCombatLogWidget::SetCategoryVisible).
	void SetChannelChecked(bool bChecked);

	UPROPERTY(BlueprintAssignable, Category = "Combat Log")
	FOnCombatLogFilterToggled OnFilterToggled;

protected:
	UPROPERTY(meta = (BindWidgetOptional)) UCheckBox*  FilterCheckBox  = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* FilterLabel_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UImage*     FilterIcon      = nullptr;

	UFUNCTION() void HandleCheckChanged(bool bIsChecked);

	ECombatLogCategory Channel = ECombatLogCategory::System;
};
