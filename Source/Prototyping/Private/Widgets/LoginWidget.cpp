// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/LoginWidget.h"

UListView* ULoginWidget::GetCharactersListView() const 
{ 
	return CharactersListView;
}

// hide the login container and show the character selection container
void ULoginWidget::ShowCharacterSelection()
{
	LoginContainer->SetVisibility(ESlateVisibility::Hidden);
	CharacterListViewContainer->SetVisibility(ESlateVisibility::Visible);
}