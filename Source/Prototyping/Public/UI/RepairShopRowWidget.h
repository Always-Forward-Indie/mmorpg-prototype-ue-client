#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "RepairShopRowWidget.generated.h"

/**
 * URepairShopRowWidget
 *
 * Base C++ class for a single row in the Repair Shop item list.
 * Create a Blueprint Widget subclass from this class in the editor and assign
 * it to the RepairRowClass property on your RepairShopWidget Blueprint.
 *
 * All widget bindings are optional so the Blueprint can omit any field it
 * does not need without generating compile errors.
 *
 * Expected named widgets in the Blueprint layout:
 *   Row_Name_Text        UTextBlock  — item display name
 *   Row_Durability_Text  UTextBlock  — "current / max" durability
 *   Row_Cost_Text        UTextBlock  — repair cost in gold
 *   Row_Repair_Btn       UButton     — triggers per-item repair
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API URepairShopRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Row_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Row_Durability_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Row_Cost_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* Row_Repair_Btn = nullptr;
};
