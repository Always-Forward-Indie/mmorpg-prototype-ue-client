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

	TSubclassOf<UDamageTextWidget> DamageTextClass;

private:
	TArray<TWeakObjectPtr<UDamageTextWidget>> WidgetPool;

	UCanvasPanel* RootCanvas = nullptr;
	APlayerController* PlayerController = nullptr;

	UDamageTextWidget* GetOrCreateWidget();
	void ReturnToPool(UDamageTextWidget* Widget);

	friend class UDamageTextWidget;

	// хендл, чтобы отписаться при уничтожении менеджера
	FDelegateHandle ReactivateHandle;

	//begin destroy
	virtual void BeginDestroy() override;
};