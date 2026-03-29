#include "UI/DialogueChoiceButton.h"

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
    if (Choice_Button)
    {
        Choice_Button->SetIsEnabled(Choice.conditionMet);
    }
}

void UDialogueChoiceButton::HandleClicked()
{
    OnChoiceButtonClicked.Broadcast(StoredEdgeId);
}
