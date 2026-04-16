#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Data/DataStructs.h"
#include "UI/QuestRewardRowWidget.h"
#include "DialogueChoiceButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChoiceButtonClicked, int32, EdgeId);

/**
 * A single clickable choice row in the dialogue widget.
 *
 * Blueprint subclass must bind:
 *   Choice_Button      → UButton    (BindWidget)
 *   Choice_Text        → UTextBlock (BindWidget)
 *
 * Optional bindings (BindWidgetOptional) for quest/turn-in/gift preview:
 *   Preview_Box        → UVerticalBox — container shown/hidden based on preview presence
 *   Preview_QuestName  → UTextBlock   — quest display name (quest/turnIn only)
 *   Preview_StepText   → UTextBlock   — first step description (quest/turnIn only)
 *   Preview_Rewards    → UTextBlock   — reward/gift summary line
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UDialogueChoiceButton : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Dialogue Choice")
    void SetupChoice(const FDialogueChoice& Choice);

    UPROPERTY(BlueprintAssignable, Category = "Dialogue Choice")
    FOnChoiceButtonClicked OnChoiceButtonClicked;

    UPROPERTY(BlueprintReadOnly, Category = "Dialogue Choice")
    int32 StoredEdgeId = 0;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleClicked();

    // ── Required bindings ────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* Choice_Button = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* Choice_Text = nullptr;

    // ── Optional quest preview bindings ──────────────────────────────────────
    /** Outer container for the entire preview section.  Hide if no preview. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UVerticalBox* Preview_Box = nullptr;

    /** Quest display name (from localization table using clientQuestKey). */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Preview_QuestName = nullptr;

    /** First step description (for offer_quest) or "Ready to turn in" (for turn_in_quest). */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Preview_StepText = nullptr;

    /** One-line reward summary fallback (used when Preview_Rewards_Box is absent). */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* Preview_Rewards = nullptr;

    /**
     * Vertical box populated with UQuestRewardRowWidget instances at runtime.
     * Preferred over Preview_Rewards text when bound and RewardRowClass is set.
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UVerticalBox* Preview_Rewards_Box = nullptr;

    /** Blueprint widget class to spawn per reward row. Assign in the BP subclass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice")
    TSubclassOf<UQuestRewardRowWidget> RewardRowClass;

private:
    void PopulatePreview(const FQuestPreviewData& Preview, bool bIsTurnIn);
    void PopulateGiftPreview(const TArray<FGiftPreviewItem>& Items);
    void PopulateRewardBox(UVerticalBox* Box, const TArray<FQuestRewardData>& Rewards);
    void PopulateGiftBox(UVerticalBox* Box, const TArray<FGiftPreviewItem>& Items);
    FString BuildRewardSummary(const TArray<FQuestRewardData>& Rewards) const;
    FString BuildGiftSummary(const TArray<FGiftPreviewItem>& Items) const;
    FText   BuildStepText(const FQuestPreviewData& Preview, bool bIsTurnIn) const;
};

