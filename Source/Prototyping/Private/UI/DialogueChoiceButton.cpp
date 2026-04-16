#include "UI/DialogueChoiceButton.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/GameInstance.h"

void UDialogueChoiceButton::NativeConstruct()
{
    Super::NativeConstruct();
    if (Choice_Button)
    {
        Choice_Button->OnClicked.AddDynamic(this, &UDialogueChoiceButton::HandleClicked);
    }
}

void UDialogueChoiceButton::SetupChoice(const FDialogueChoice& Choice)
{
    StoredEdgeId = Choice.edgeId;

    // ── Choice text ──────────────────────────────────────────────────────
    if (Choice_Text)
    {
        ULocalizationSubsystem* LocSys = nullptr;
        if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

        FText ChoiceText;
        if (LocSys && !Choice.clientChoiceKey.IsEmpty())
        {
            ChoiceText = LocSys->GetDialogueChoiceText(Choice.clientChoiceKey);
        }
        else
        {
            ChoiceText = FText::FromString(Choice.clientChoiceKey);
        }
        Choice_Text->SetText(ChoiceText);
        Choice_Text->SetColorAndOpacity(FSlateColor(
            Choice.conditionMet ? FLinearColor::White : FLinearColor(0.4f, 0.4f, 0.4f, 1.f)));
    }

    // Disable or hide the button based on conditionMet / hideIfLocked
    if (Choice_Button)
    {
        if (!Choice.conditionMet && Choice.hideIfLocked)
        {
            SetVisibility(ESlateVisibility::Collapsed);
            return;
        }
        Choice_Button->SetIsEnabled(Choice.conditionMet);
    }

    // ── Quest / Turn-in / Gift preview ───────────────────────────────────────
    if (Choice.bHasQuestPreview)
    {
        PopulatePreview(Choice.questPreview, /*bIsTurnIn=*/false);
    }
    else if (Choice.bHasTurnInPreview)
    {
        PopulatePreview(Choice.turnInPreview, /*bIsTurnIn=*/true);
    }
    else if (Choice.bHasGiftPreview)
    {
        PopulateGiftPreview(Choice.giftPreview);
    }
    else
    {
        if (Preview_Box) Preview_Box->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueChoiceButton::PopulatePreview(const FQuestPreviewData& Preview, bool bIsTurnIn)
{
    if (!Preview_Box)
    {
        return;
    }
    Preview_Box->SetVisibility(ESlateVisibility::HitTestInvisible);

    ULocalizationSubsystem* LocSys = nullptr;
    if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

    if (Preview_QuestName)
    {
        FText QuestName;
        if (LocSys && !Preview.clientQuestKey.IsEmpty())
        {
            QuestName = LocSys->GetQuestDisplayName(Preview.clientQuestKey);
        }
        else
        {
            QuestName = FText::FromString(Preview.clientQuestKey);
        }
        Preview_QuestName->SetText(QuestName);
    }

    if (Preview_StepText)
    {
        Preview_StepText->SetText(BuildStepText(Preview, bIsTurnIn));
    }

    if (Preview_Rewards)
    {
        if (Preview_Rewards_Box && RewardRowClass)
        {
            // List mode takes over; hide the legacy text element
            Preview_Rewards->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            Preview_Rewards->SetText(FText::FromString(BuildRewardSummary(Preview.rewards)));
        }
    }

    if (Preview_Rewards_Box && RewardRowClass)
    {
        PopulateRewardBox(Preview_Rewards_Box, Preview.rewards);
    }
}

FText UDialogueChoiceButton::BuildStepText(const FQuestPreviewData& Preview, bool bIsTurnIn) const
{
    if (bIsTurnIn)
    {
        return FText::FromString(TEXT("Quest ready to turn in"));
    }

    if (!Preview.bHasFirstStep)
    {
        return FText::GetEmpty();
    }

    ULocalizationSubsystem* LocSys = nullptr;
    if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

    const FQuestStepEnrichedData& Step = Preview.firstStep;

    // Try localised step description first
    if (LocSys && !Step.clientStepKey.IsEmpty())
    {
        FText Desc = LocSys->GetQuestStepDescription(Step.clientStepKey);
        if (!Desc.IsEmpty()) return Desc;
    }

    // Build a readable fallback from enriched data
    if (Step.stepType == TEXT("kill"))
    {
        FText TargetName;
        if (LocSys && !Step.targetSlug.IsEmpty())
        {
            TargetName = LocSys->GetMobDisplayName(Step.targetSlug);
        }
        else
        {
            TargetName = FText::FromString(Step.targetSlug);
        }
        return FText::Format(FText::FromString(TEXT("Kill: {0} × {1}")), TargetName, FText::AsNumber(Step.count));
    }
    else if (Step.stepType == TEXT("collect"))
    {
        FText TargetName;
        if (LocSys && !Step.targetSlug.IsEmpty())
        {
            TargetName = LocSys->GetItemDisplayName(Step.targetSlug);
        }
        else
        {
            TargetName = FText::FromString(Step.targetSlug);
        }
        return FText::Format(FText::FromString(TEXT("Collect: {0} × {1}")), TargetName, FText::AsNumber(Step.count));
    }
    else if (Step.stepType == TEXT("talk"))
    {
        FText TargetName;
        if (LocSys && !Step.targetSlug.IsEmpty())
        {
            TargetName = LocSys->GetNPCDisplayName(Step.targetSlug);
        }
        else
        {
            TargetName = FText::FromString(Step.targetSlug);
        }
        return FText::Format(FText::FromString(TEXT("Talk to: {0}")), TargetName);
    }
    else if (Step.stepType == TEXT("reach"))
    {
        FText ZoneName;
        if (LocSys && !Step.zoneSlug.IsEmpty())
        {
            ZoneName = LocSys->GetZoneDisplayName(Step.zoneSlug);
        }
        else
        {
            ZoneName = FText::FromString(Step.zoneSlug);
        }
        return FText::Format(FText::FromString(TEXT("Go to: {0}")), ZoneName);
    }

    return FText::FromString(Step.clientStepKey);
}

void UDialogueChoiceButton::PopulateRewardBox(UVerticalBox* Box, const TArray<FQuestRewardData>& Rewards)
{
    Box->ClearChildren();
    for (const FQuestRewardData& R : Rewards)
    {
        UQuestRewardRowWidget* Row = CreateWidget<UQuestRewardRowWidget>(GetOwningPlayer(), RewardRowClass);
        if (Row)
        {
            Row->SetupFromQuestReward(R);
            Box->AddChild(Row);
        }
    }
    Box->SetVisibility(Rewards.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UDialogueChoiceButton::PopulateGiftBox(UVerticalBox* Box, const TArray<FGiftPreviewItem>& Items)
{
    Box->ClearChildren();
    for (const FGiftPreviewItem& G : Items)
    {
        UQuestRewardRowWidget* Row = CreateWidget<UQuestRewardRowWidget>(GetOwningPlayer(), RewardRowClass);
        if (Row)
        {
            Row->SetupFromGiftItem(G);
            Box->AddChild(Row);
        }
    }
    Box->SetVisibility(Items.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

FString UDialogueChoiceButton::BuildRewardSummary(const TArray<FQuestRewardData>& Rewards) const
{
    if (Rewards.IsEmpty()) return TEXT("");

    ULocalizationSubsystem* LocSys = nullptr;
    if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

    TArray<FString> Parts;
    for (const FQuestRewardData& R : Rewards)
    {
        if (R.rewardType == TEXT("exp"))
        {
            Parts.Add(FString::Printf(TEXT("• %d EXP"), R.amount));
        }
        else if (R.rewardType == TEXT("gold"))
        {
            Parts.Add(FString::Printf(TEXT("• %d Gold"), R.amount));
        }
        else if (R.rewardType == TEXT("item"))
        {
            if (R.isHidden)
            {
                Parts.Add(TEXT("• ???"));
            }
            else
            {
                FString ItemName;
                if (LocSys && !R.itemSlug.IsEmpty())
                {
                    ItemName = LocSys->GetItemDisplayName(R.itemSlug).ToString();
                }
                else
                {
                    ItemName = R.itemSlug;
                }
                Parts.Add(FString::Printf(TEXT("• %d× %s"), R.quantity, *ItemName));
            }
        }
    }
    return FString::Join(Parts, TEXT("  "));
}

void UDialogueChoiceButton::PopulateGiftPreview(const TArray<FGiftPreviewItem>& Items)
{
    if (!Preview_Box) return;
    Preview_Box->SetVisibility(ESlateVisibility::HitTestInvisible);

    // Gift preview has no quest name or step text — only a reward/gift line
    if (Preview_QuestName) Preview_QuestName->SetVisibility(ESlateVisibility::Collapsed);
    if (Preview_StepText)  Preview_StepText->SetVisibility(ESlateVisibility::Collapsed);

    if (Preview_Rewards)
    {
        if (Preview_Rewards_Box && RewardRowClass)
        {
            Preview_Rewards->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            Preview_Rewards->SetVisibility(ESlateVisibility::HitTestInvisible);
            Preview_Rewards->SetText(FText::FromString(BuildGiftSummary(Items)));
        }
    }

    if (Preview_Rewards_Box && RewardRowClass)
    {
        PopulateGiftBox(Preview_Rewards_Box, Items);
    }
}

FString UDialogueChoiceButton::BuildGiftSummary(const TArray<FGiftPreviewItem>& Items) const
{
    if (Items.IsEmpty()) return TEXT("");

    ULocalizationSubsystem* LocSys = nullptr;
    if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

    TArray<FString> Parts;
    for (const FGiftPreviewItem& G : Items)
    {
        if (G.giftType == TEXT("exp"))
        {
            Parts.Add(FString::Printf(TEXT("• %d EXP"), G.amount));
        }
        else if (G.giftType == TEXT("gold"))
        {
            Parts.Add(FString::Printf(TEXT("• %d Gold"), G.amount));
        }
        else if (G.giftType == TEXT("item"))
        {
            FString ItemName;
            if (LocSys && !G.itemSlug.IsEmpty())
            {
                ItemName = LocSys->GetItemDisplayName(G.itemSlug).ToString();
            }
            else
            {
                ItemName = G.itemSlug.IsEmpty() ? TEXT("???") : G.itemSlug;
            }
            Parts.Add(FString::Printf(TEXT("• %d× %s"), G.quantity, *ItemName));
        }
    }
    return FString::Join(Parts, TEXT("  "));
}

void UDialogueChoiceButton::HandleClicked()
{
    OnChoiceButtonClicked.Broadcast(StoredEdgeId);
}
