#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Data/DataStructs.h"
#include "DialogueChoiceButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChoiceButtonClicked, int32, EdgeId);

/**
 * A single clickable choice row in the dialogue widget.
 * Blueprint subclass must bind:
 *   Choice_Button  ? UButton    (BindWidget)
 *   Choice_Text    ? UTextBlock (BindWidget)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UDialogueChoiceButton : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Dialogue Choice")
    void SetupChoice(const FDialogueChoice& Choice);

    UPROPERTY(BlueprintAssignable, Category = "Dialogue Choice")
    FOnChoiceButtonClicked OnChoiceButtonClicked;

    UPROPERTY(BlueprintReadOnly, Category = "Dialogue Choice")
    int32 StoredEdgeId = 0;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleClicked();

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* Choice_Button = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Choice_Text = nullptr;
};
