#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Data/DataStructs.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "QuestRewardRowWidget.generated.h"

/**
 * A single row in a quest/gift rewards list.
 *
 * Displays: optional icon + reward name + amount/quantity.
 *
 * Blueprint subclass must bind:
 *   Reward_Name_Text   → UTextBlock (BindWidget)
 *
 * Optional bindings:
 *   Reward_Icon        → UImage    (BindWidgetOptional) — item icon; hidden for exp/gold
 *   Reward_Amount_Text → UTextBlock (BindWidgetOptional) — e.g. "×3" or "500"
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UQuestRewardRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Quest Reward Row")
    void SetupFromQuestReward(const FQuestRewardData& Reward);

    UFUNCTION(BlueprintCallable, Category = "Quest Reward Row")
    void SetupFromGiftItem(const FGiftPreviewItem& Gift);

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Reward_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* Reward_Icon = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Reward_Amount_Text = nullptr;

private:
    void Populate(const FString& TypeOrSlug, int32 Amount, bool bIsItem, bool bIsHidden);
    void LoadItemIcon(const FString& ItemSlug);
    void AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback);

    TSharedPtr<FStreamableHandle> StreamableHandle;
};
