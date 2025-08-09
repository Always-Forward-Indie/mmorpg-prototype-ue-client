// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/LoginWidget.h"

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

void ULoginWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (AccountComboBox)
    {
        AccountComboBox->ClearOptions();
        AccountComboBox->AddOption(TEXT("TestUser1"));
        AccountComboBox->AddOption(TEXT("TestUser2"));
        AccountComboBox->AddOption(TEXT("TestUser3"));

        AccountComboBox->OnSelectionChanged.AddDynamic(this, &ULoginWidget::OnAccountSelected);
    }
}

void ULoginWidget::OnAccountSelected(FString SelectedItem, ESelectInfo::Type SelectType)
{
    if (!LoginInput || !PasswordInput)
        return;

    if (SelectedItem == "TestUser1")
    {
        LoginInput->SetText(FText::FromString("test1"));
        PasswordInput->SetText(FText::FromString("test1"));
    }
    else if (SelectedItem == "TestUser2")
    {
        LoginInput->SetText(FText::FromString("test2"));
        PasswordInput->SetText(FText::FromString("test2"));
    }
    else if (SelectedItem == "TestUser3")
    {
        LoginInput->SetText(FText::FromString("test3"));
        PasswordInput->SetText(FText::FromString("test3"));
    }
}
