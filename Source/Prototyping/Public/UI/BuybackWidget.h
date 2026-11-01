#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Data/DataStructs.h"
#include "BuybackWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBuybackRowClicked, int32, BuybackSlot, int32, Quantity);

class UBuybackWidget;

/**
 * Row click binding (defined first so UBuybackWidget can hold TArray of it).
 */
UCLASS()
class PROTOTYPING_API UBuybackRowBinding : public UObject
{
	GENERATED_BODY()

public:
	void Setup(UBuybackWidget* InWidget, int32 InSlot, int32 InQuantity);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY() UBuybackWidget* Widget = nullptr;
	int32 BoundSlot = 0;
	int32 Quantity = 0;
};

/**
 * UBuybackWidget
 *
 * Runtime-built buyback list (last sold stacks, full-stack repurchase).
 * Content comes from UVendorManager (buybackList / buybackResult);
 * row clicks go out via OnBuybackRowClicked.
 *
 * Blueprint layout (optional): create a BP subclass, bind these names,
 * assign it to UVendorShopWidget.BuybackWidgetClass. Bind all three or
 * none — rows themselves are always built in code (uniform styling):
 *   Buyback_Title_Text  UTextBlock  header ("Buyback" / empty-state line)
 *   Buyback_List_Box    UScrollBox  entry rows go here
 *   Buyback_Close_Btn   UButton     hides the window
 * With no subclass assigned the whole layout is built in C++ (headless-safe).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UBuybackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Rebuild the list and show the window. */
	UFUNCTION(BlueprintCallable, Category = "Buyback")
	void ShowBuyback(const TArray<FBuybackEntry>& Entries);

	UFUNCTION(BlueprintCallable, Category = "Buyback")
	void HideBuyback();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Buyback")
	bool IsBuybackOpen() const;

	UPROPERTY(BlueprintAssignable, Category = "Buyback")
	FOnBuybackRowClicked OnBuybackRowClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buyback")
	FVector2D WindowSize = FVector2D(360.f, 420.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buyback")
	FVector2D WindowPosition = FVector2D(60.f, -40.f);

	// Blueprint layout slots (BindWidgetOptional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Buyback_Title_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* Buyback_List_Box = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Buyback_Close_Btn = nullptr;

private:
	class UScrollBox* ListBox = nullptr;
	class UTextBlock* HeaderText = nullptr;
	class UVerticalBox* RootCol = nullptr;
	UPROPERTY() TArray<UBuybackRowBinding*> RowBindings;

	void BuildLayout();
	void ClearList();
	void AddEntryRow(const FBuybackEntry& Entry, const FString& DisplayName);
	FString LocalizeItem(const FString& Slug) const;

	// Wiring happens once (NativeConstruct may re-run on re-add to viewport).
	bool bWired = false;
};
