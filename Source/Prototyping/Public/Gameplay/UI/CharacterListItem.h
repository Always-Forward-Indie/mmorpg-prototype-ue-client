// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "CharacterListItem.generated.h"

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UCharacterListItem : public UUserWidget
{
	GENERATED_BODY()
	
public:
    /** Sets the character name to be displayed in this list item. */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void SetCharacterItemData(const FString& Name, const int32 ID);

    // get the character name
    UTextBlock* GetCharacterItemLabel() const;

    // uproperty for the character name
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    FText CharacterName;

    // uproperty for the character id
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
    int32 CharacterID;

public:
    /** Reference to the text block widget for displaying the character name. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* CharacterNameTextBlock;
};
