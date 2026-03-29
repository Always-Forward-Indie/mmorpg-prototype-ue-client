// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/W_NPCNameplateWidget.h"

void UW_NPCNameplateWidget::SetNPCInfo(const FString&        InName,
                                        const FString&        InNPCType,
                                        int32                 InLevel,
                                        ENPCInteractionState  InteractionState)
{
    CachedInteractionState = InteractionState;
    bCachedInteractable    = (InteractionState != ENPCInteractionState::NotInteractable);

    // --- Name ---
    if (NPCNameText)
    {
        NPCNameText->SetText(FText::FromString(InName));
        const FLinearColor NameColor = bCachedInteractable ? InteractableNameColor : NonInteractableNameColor;
        NPCNameText->SetColorAndOpacity(FSlateColor(NameColor));
    }

    // --- Type label ---
    if (NPCTypeText)
    {
        if (!InNPCType.IsEmpty() && bCachedInteractable)
        {
            NPCTypeText->SetText(FText::FromString(TypePrefix + InNPCType + TypeSuffix));
            NPCTypeText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            NPCTypeText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Level ---
    if (NPCLevelText)
    {
        if (InLevel > 0)
        {
            NPCLevelText->SetText(FText::FromString(
                LevelFormat.Replace(TEXT("{0}"), *FString::FromInt(InLevel))));
            NPCLevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            NPCLevelText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // --- Interaction state indicators ---
    // Priority: QuestComplete > QuestAvailable > QuestInProgress > DialogueOnly > None
    auto ShowOnly = [](UImage* Target, bool bShow)
    {
        if (Target)
        {
            Target->SetVisibility(bShow
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
        }
    };

    ShowOnly(QuestIndicatorImage,  InteractionState == ENPCInteractionState::QuestAvailable);
    ShowOnly(QuestDoneImage,       InteractionState == ENPCInteractionState::QuestComplete);
    ShowOnly(QuestInProgressImage, InteractionState == ENPCInteractionState::QuestInProgress);
    ShowOnly(DialogueIndicatorImage, InteractionState == ENPCInteractionState::DialogueOnly);

    // Start bobbing animation if any quest icon is now active
    const bool bHasQuestIcon = (InteractionState == ENPCInteractionState::QuestAvailable
                              || InteractionState == ENPCInteractionState::QuestComplete
                              || InteractionState == ENPCInteractionState::QuestInProgress
                              || InteractionState == ENPCInteractionState::DialogueOnly);
    if (bHasQuestIcon)
    {
        PlayQuestIconAnimation();
    }
    else
    {
        StopQuestIconAnimation();
    }

    // Interact hint starts hidden; SetPlayerInRange will show it
    if (InteractHintText)
    {
        InteractHintText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UW_NPCNameplateWidget::SetPlayerInRange(bool bInRange)
{
    if (!InteractHintText)
    {
        return;
    }

    if (bInRange && bCachedInteractable)
    {
        InteractHintText->SetText(FText::FromString(InteractHintString));
        InteractHintText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        InteractHintText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UW_NPCNameplateWidget::SetWidgetScale(float Scale)
{
    FWidgetTransform Transform = GetRenderTransform();
    Transform.Scale = FVector2D(Scale, Scale);
    SetRenderTransform(Transform);
}

void UW_NPCNameplateWidget::SetNameplateOpacity(float Opacity)
{
    SetRenderOpacity(Opacity);
}

void UW_NPCNameplateWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Animation starts only when an icon becomes visible via SetNPCInfo.
}

void UW_NPCNameplateWidget::PlayQuestIconAnimation()
{
    if (!QuestIconBob)
    {
        return;
    }

    // If already playing, leave it — avoid restart that causes a visual pop.
    if (IsAnimationPlaying(QuestIconBob))
    {
        return;
    }

    PlayAnimation(QuestIconBob, 0.f, 0 /* loop forever */, EUMGSequencePlayMode::Forward, 1.0f);
}

void UW_NPCNameplateWidget::StopQuestIconAnimation()
{
    if (QuestIconBob && IsAnimationPlaying(QuestIconBob))
    {
        StopAnimation(QuestIconBob);
    }
}

UImage* UW_NPCNameplateWidget::GetActiveQuestIcon() const
{
    if (QuestIndicatorImage  && QuestIndicatorImage->GetVisibility()  != ESlateVisibility::Collapsed) return QuestIndicatorImage;
    if (QuestDoneImage       && QuestDoneImage->GetVisibility()       != ESlateVisibility::Collapsed) return QuestDoneImage;
    if (QuestInProgressImage && QuestInProgressImage->GetVisibility() != ESlateVisibility::Collapsed) return QuestInProgressImage;
    if (DialogueIndicatorImage && DialogueIndicatorImage->GetVisibility() != ESlateVisibility::Collapsed) return DialogueIndicatorImage;
    return nullptr;
}
