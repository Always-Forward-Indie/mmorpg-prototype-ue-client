#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "InputAction.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "InteractionHintWidget.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UInteractionHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void ShowHint(EInteractableType Type, bool bShowHotkey = true);
	void HideHint();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hint|Binding")
	UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hint|Templates")
	FText Template_DroppedItem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hint|Templates")
	FText Template_MobHarvestable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hint|Templates")
	FText Template_MobHarvested;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hint|Templates")
	FText Template_NPC;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hint|Templates")
	FText Template_RemotePlayer;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* HintText;

private:
	FString ResolvedKeyBinding;

	void ResolveKeyBinding();
	const FText& GetTemplate(EInteractableType Type) const;
};
