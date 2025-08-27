#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "FloatingCombatTextManager.generated.h"

class UDamageTextWidget;
class UCanvasPanel;

UCLASS()
class PROTOTYPING_API UFloatingCombatTextManager : public UObject
{
	GENERATED_BODY()

public:
	void Init(UCanvasPanel* InCanvas, APlayerController* InPC, TSubclassOf<UDamageTextWidget> InDamageTextClass);

	void ShowDamage(const FVector& WorldLocation, float Damage, bool bIsCrit, EDamageType DamageType);

	void ShowMissText(const FVector& WorldLocation);
	void ShowBlockedText(const FVector& WorldLocation);

	TSubclassOf<UDamageTextWidget> DamageTextClass;

	// Validation getter methods
	UCanvasPanel* GetRootCanvas() const;
	APlayerController* GetPlayerController() const;
	TSubclassOf<UDamageTextWidget> GetDamageTextClass() const;

private:
	TArray<TWeakObjectPtr<UDamageTextWidget>> WidgetPool;

	UCanvasPanel* RootCanvas = nullptr;
	APlayerController* PlayerController = nullptr;

	UDamageTextWidget* GetOrCreateWidget();
	void ReturnToPool(UDamageTextWidget* Widget);

	void ShowSpecialText(const FVector& WorldLocation, const FString& Text, FLinearColor Color);

	friend class UDamageTextWidget;

	FDelegateHandle ReactivateHandle;

	virtual void BeginDestroy() override;
};