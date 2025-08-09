// UIManager.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "UIManager.generated.h"


class UFloatingCombatTextManager;
class UCanvasPanel;
class UDamageTextWidget;

UCLASS()
class PROTOTYPING_API UUIManager : public UObject
{
	GENERATED_BODY()

public:
	void Init(APlayerController* InPC, UCanvasPanel* InRootCanvas, 
		TSubclassOf<UDamageTextWidget> InDamageTextClass);

	UFUNCTION()
	UFloatingCombatTextManager* GetFCTManager() const { return FCTManager; }


private:
	UPROPERTY()
	UFloatingCombatTextManager* FCTManager;

	UPROPERTY()
	APlayerController* PlayerController;

	UPROPERTY()
	UCanvasPanel* RootCanvas;
};
