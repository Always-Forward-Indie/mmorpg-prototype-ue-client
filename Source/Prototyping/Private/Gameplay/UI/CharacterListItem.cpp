// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/CharacterListItem.h"

void UCharacterListItem::SetCharacterItemData(const FString& Name, const int32 ID)
{
    if (CharacterNameTextBlock)
    {
        UE_LOG(LogTemp, Warning, TEXT("CharacterListItem::SetCharacterItemLabel: %s"), *Name);
        CharacterName = FText::FromString(Name);
        CharacterID = ID;
    }
}

UTextBlock* UCharacterListItem::GetCharacterItemLabel() const
{
	return CharacterNameTextBlock;
}