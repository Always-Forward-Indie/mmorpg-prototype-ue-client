#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Gameplay/Social/PlayerInspectManager.h"
#include "PlayerInspectWidget.generated.h"

/**
 * UPlayerInspectWidget
 *
 * Read-only snapshot window for unit-menu inspect: identity header
 * (name / level / class / race) + equipped item rows with localized
 * names. Layout is built in C++ — works with zero Blueprint setup.
 * Never sends packets; content comes only from UPlayerInspectManager.
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

private:
	class UScrollBox* GearBox = nullptr;
	class UTextBlock* HeaderText = nullptr;
	class UVerticalBox* RootCol = nullptr;

	void BuildLayout();
	void ClearGear();
	void AddGearRow(const FString& SlotName, const FString& ItemName);
	FString LocalizeItem(const FString& Slug) const;
};
