// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/ListView.h>
#include "LoginWidget.generated.h"


/**
 * 
 */
UCLASS()
class PROTOTYPING_API ULoginWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Reference to the character list view component. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UListView* CharactersListView;

public:
	// get the character list view
	UListView* GetCharactersListView() const;
};
