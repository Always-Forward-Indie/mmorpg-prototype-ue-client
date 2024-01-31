// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/ListView.h>
#include <Components/CanvasPanel.h>
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

	/** Reference to the character list view component. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCanvasPanel* LoginContainer;

	/** Reference to the character list view component. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCanvasPanel* CharacterListViewContainer;

public:
	// get the character list view
	UListView* GetCharactersListView() const;

	
	// hide the login container
	void ShowCharacterSelection();
};
