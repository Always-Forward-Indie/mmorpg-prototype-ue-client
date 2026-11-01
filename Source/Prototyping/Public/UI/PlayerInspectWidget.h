#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Data/DataStructs.h"
#include "Gameplay/Social/PlayerInspectManager.h"
#include "PlayerInspectWidget.generated.h"

/**
 * UPlayerInspectWidget
 *
 * Read-only snapshot window for unit-menu inspect: identity header
 * (name / level / class / race) + equipped item rows with localized
 * names. Never sends packets; content comes only from UPlayerInspectManager.
 *
 * Blueprint layout (optional): create a BP subclass, bind these names,
 * assign it to UIManager.PlayerInspectWidgetClass. Bind all three or none —
 * rows are always built in code (uniform styling):
 *   Inspect_Title_Text  UTextBlock  "Name [Level] Class" header
 *   Inspect_Gear_Box    UScrollBox  gear/attribute rows go here
 *   Inspect_Close_Btn   UButton     hides the window
 * With no subclass assigned the whole layout is built in C++ (headless-safe).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerInspectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void ShowInspect(const FPlayerInspectData& Data);

	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void HideInspect();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inspect")
	bool IsInspectOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FVector2D WindowSize = FVector2D(340.f, 420.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FVector2D WindowPosition = FVector2D(60.f, -40.f);

	// Blueprint layout slots (BindWidgetOptional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Inspect_Title_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* Inspect_Gear_Box = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Inspect_Close_Btn = nullptr;

private:
	class UScrollBox* GearBox = nullptr;
	class UTextBlock* HeaderText = nullptr;
	class UVerticalBox* RootCol = nullptr;

	void BuildLayout();
	void ClearGear();
	void AddGearRow(const FString& SlotName, const FString& ItemName);
	FString LocalizeItem(const FString& Slug) const;

	// Wiring happens once (NativeConstruct may re-run on re-add to viewport).
	bool bWired = false;
};
