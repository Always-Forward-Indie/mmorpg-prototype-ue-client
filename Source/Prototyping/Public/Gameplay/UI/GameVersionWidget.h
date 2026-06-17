#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "GameVersionWidget.generated.h"

UCLASS()
class PROTOTYPING_API UGameVersionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* VersionText;

protected:
	virtual void NativeConstruct() override;
};
