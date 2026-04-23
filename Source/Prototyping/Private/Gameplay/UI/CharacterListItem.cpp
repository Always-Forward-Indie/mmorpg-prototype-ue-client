// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/CharacterListItem.h"

void UCharacterListItem::SetCharacterItemData(const FString& Name, const int32 ID)
{
    CharacterName = FText::FromString(Name);
    CharacterID = ID;

    if (CharacterNameTextBlock)
    {
        CharacterNameTextBlock->SetText(CharacterName);
    }
}

UTextBlock* UCharacterListItem::GetCharacterItemLabel() const
{
	return CharacterNameTextBlock;
}

void UCharacterListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
    // ListView passes the object that was AddItem'd as the data source.
    // Cast it back to UCharacterListItem to read the pre-filled fields.
    if (const UCharacterListItem* Source = Cast<UCharacterListItem>(ListItemObject))
    {
        CharacterName = Source->CharacterName;
        CharacterID   = Source->CharacterID;

        if (CharacterNameTextBlock)
        {
            CharacterNameTextBlock->SetText(CharacterName);
        }
    }
}

void UCharacterListItem::NativeOnItemSelectionChanged(bool bIsSelected)
{
    if (CharacterNameTextBlock)
    {
        CharacterNameTextBlock->SetColorAndOpacity(bIsSelected ? SelectedTextColor : NormalTextColor);
    }
    if (Img_SelectionHighlight)
    {
        Img_SelectionHighlight->SetColorAndOpacity(bIsSelected ? SelectedHighlightColor : NormalHighlightColor);
    }
    OnSelectionChanged(bIsSelected);
}
