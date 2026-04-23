// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "CharacterListItem.generated.h"

/**
 * List-entry widget for the character-select ListView.
 *
 * Implements IUserObjectListEntry so the ListView can notify this widget
 * when its selection state changes.  Override OnSelectionChanged in the
 * Blueprint child (WBP_CharacterListItem) to update colours / borders etc.
 */
UCLASS()
class PROTOTYPING_API UCharacterListItem : public UUserWidget, public IUserObjectListEntry
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

    /**
     * Called by the ListView whenever this item's selection state changes.
     * Fires after C++ has already applied the colour properties below, so you
     * can do additional Blueprint logic here if needed.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Character")
    void OnSelectionChanged(bool bIsSelected);

    // ------------------------------------------------------------------
    // Visual configurator — edit these in the Blueprint Class Defaults.
    // No Blueprint event override needed for basic highlighting.
    // ------------------------------------------------------------------

    /** Text colour when the item is NOT selected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection")
    FLinearColor NormalTextColor = FLinearColor::White;

    /** Text colour when the item IS selected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection")
    FLinearColor SelectedTextColor = FLinearColor(1.0f, 0.75f, 0.0f, 1.0f);

    /**
     * Tint applied to Img_SelectionHighlight when NOT selected.
     * Set alpha to 0 to hide the image entirely when unselected.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection")
    FLinearColor NormalHighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);

    /** Tint applied to Img_SelectionHighlight when selected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection")
    FLinearColor SelectedHighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);

public:
    /** Required text block — must be named CharacterNameTextBlock in the Blueprint. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* CharacterNameTextBlock;

    /**
     * Optional full-row highlight image — add an Image widget named
     * "Img_SelectionHighlight" to the Blueprint to enable it.
     * Its colour is driven by NormalHighlightColor / SelectedHighlightColor.
     */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* Img_SelectionHighlight;

protected:
    // IUserObjectListEntry
    virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
