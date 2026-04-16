#include "UI/DialogueWidget.h"
#include "UI/DialogueChoiceButton.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "MyGameInstance.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/GameInstance.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
    {
        Close_Button->OnClicked.AddDynamic(this, &UDialogueWidget::HandleCloseButtonClicked);
    }

    // Position window in the center of the viewport initially
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
        const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D(
            FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
            FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void UDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
}

void UDialogueWidget::BindToDialogueManager(UDialogueManager* InDialogueManager)
{
    if (!InDialogueManager)
    {
        return;
    }

    // Unbind from any previous manager
    if (DialogueManager)
    {
        DialogueManager->OnDialogueNodeReceived.RemoveDynamic(this, &UDialogueWidget::HandleDialogueNodeReceived);
        DialogueManager->OnDialogueSessionClosed.RemoveDynamic(this, &UDialogueWidget::HandleDialogueSessionClosed);
        DialogueManager->OnDialogueError.RemoveDynamic(this, &UDialogueWidget::HandleDialogueError);
    }

    DialogueManager = InDialogueManager;

    DialogueManager->OnDialogueNodeReceived.AddDynamic(this, &UDialogueWidget::HandleDialogueNodeReceived);
    DialogueManager->OnDialogueSessionClosed.AddDynamic(this, &UDialogueWidget::HandleDialogueSessionClosed);
    DialogueManager->OnDialogueError.AddDynamic(this, &UDialogueWidget::HandleDialogueError);
}

void UDialogueWidget::ShowDialogueNode(const FDialogueNodeData& NodeData)
{
    CurrentNode = NodeData;

    // NPC name: resolve via NPCManager + LocalizationSubsystem, fallback to raw name / "NPC #id"
    if (NPC_Name_Text)
    {
        FString NPCName = FString::Printf(TEXT("NPC #%d"), NodeData.speakerNpcId);
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(GI))
            {
                if (UNPCManager* NPCMgr = MyGI->GetNPCManager())
                {
                    if (ABasicNPC* NPC = NPCMgr->GetNPCById(NodeData.speakerNpcId))
                    {
                        ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>();
                        if (LocSys && !NPC->GetNPCSlug().IsEmpty())
                        {
                            const FText LocalizedName = LocSys->GetNPCDisplayName(NPC->GetNPCSlug());
                            NPCName = LocalizedName.IsEmpty() ? NPC->GetNPCName() : LocalizedName.ToString();
                        }
                        else
                        {
                            NPCName = NPC->GetNPCName();
                        }
                    }
                }
            }
        }
        NPC_Name_Text->SetText(FText::FromString(NPCName));
    }

    // Track the active speaker so farewell fires on close.
    // If this is a new session (widget was hidden), trigger greeting on the NPC first.
    const bool bNewSession = (CurrentSpeakerNpcId == 0);
    CurrentSpeakerNpcId = NodeData.speakerNpcId;

    if (bNewSession && CurrentSpeakerNpcId > 0)
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(GI))
            {
                if (UNPCManager* NPCMgr = MyGI->GetNPCManager())
                {
                    if (ABasicNPC* NPC = NPCMgr->GetNPCById(CurrentSpeakerNpcId))
                    {
                        NPC->NotifyWindowOpened(); // increment before PlayGreetingSound
                        NPC->PlayGreetingSound();
                    }
                }
            }
        }
    }

    // Node text: look up localised text by clientNodeKey via LocalizationSubsystem;
    // fall back to raw key during development if table is not yet configured.
    if (Dialogue_Text)
    {
        ULocalizationSubsystem* LocSys = nullptr;
        if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

        FText NodeText;
        if (LocSys && !NodeData.clientNodeKey.IsEmpty())
        {
            NodeText = LocSys->GetDialogueNodeText(NodeData.clientNodeKey);
        }
        else
        {
            NodeText = FText::FromString(NodeData.clientNodeKey);
        }
        // Convert literal \n (backslash + n) written in DataTable to actual newline chars
        FString NodeStr = NodeText.ToString();
        NodeStr = NodeStr.Replace(TEXT("\\n"), TEXT("\n"));
        Dialogue_Text->SetText(FText::FromString(NodeStr));
    }

    PopulateChoices(NodeData.choices);

    SetVisibility(ESlateVisibility::Visible);
    OnDialogueVisibilityChanged.Broadcast(true);
}

void UDialogueWidget::ShowError(const FDialogueErrorData& ErrorData)
{
    // Show a brief error in the Dialogue_Text field and hide after a moment
    if (Dialogue_Text)
    {
        Dialogue_Text->SetText(FText::FromString(
            FString::Printf(TEXT("[Error] %s"), *ErrorData.errorCode)));
    }
    if (Choices_Box)
    {
        Choices_Box->ClearChildren();
    }
    SetVisibility(ESlateVisibility::Visible);
}

void UDialogueWidget::HideDialogue()
{
    // Notify the NPC that one interaction window closed.
    // Farewell plays only when ALL NPC windows (dialogue + shops) are closed.
    if (CurrentSpeakerNpcId > 0)
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(GI))
            {
                if (UNPCManager* NPCMgr = MyGI->GetNPCManager())
                {
                    if (ABasicNPC* NPC = NPCMgr->GetNPCById(CurrentSpeakerNpcId))
                    {
                        NPC->NotifyWindowClosed();
                    }
                }
            }
        }
        CurrentSpeakerNpcId = 0;
    }

    if (Choices_Box)
    {
        Choices_Box->ClearChildren();
    }
    SetVisibility(ESlateVisibility::Collapsed);
    OnDialogueVisibilityChanged.Broadcast(false);
}

void UDialogueWidget::PopulateChoices(const TArray<FDialogueChoice>& Choices)
{
    if (!Choices_Box)
    {
        return;
    }
    Choices_Box->ClearChildren();
    PendingChoiceEdgeIds.Reset();

    for (const FDialogueChoice& Choice : Choices)
    {
        // hideIfLocked is authoritative — always skip if server tagged it hidden when locked
        if (!Choice.conditionMet && Choice.hideIfLocked)
        {
            continue;
        }
        if (!Choice.conditionMet && !bShowLockedChoices)
        {
            continue;
        }

        // Preferred: ChoiceButtonClass is a UDialogueChoiceButton subclass
        if (ChoiceButtonClass && ChoiceButtonClass->IsChildOf(UDialogueChoiceButton::StaticClass()))
        {
            UDialogueChoiceButton* ChoiceBtn = CreateWidget<UDialogueChoiceButton>(
                GetOwningPlayer(), ChoiceButtonClass);
            if (ChoiceBtn)
            {
                ChoiceBtn->SetupChoice(Choice);
                ChoiceBtn->OnChoiceButtonClicked.AddDynamic(this, &UDialogueWidget::HandleChoiceButtonClicked);
                Choices_Box->AddChild(ChoiceBtn);
            }
        }
        else if (ChoiceButtonClass)
        {
            // Generic UUserWidget � look for inner button and text by name
            UUserWidget* BtnWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), ChoiceButtonClass);
            if (BtnWidget)
            {
                UTextBlock* TextBlock = Cast<UTextBlock>(BtnWidget->GetWidgetFromName(TEXT("Choice_Text")));
                if (TextBlock)
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
                    TextBlock->SetText(ChoiceText);
                    TextBlock->SetColorAndOpacity(FSlateColor(
                        Choice.conditionMet ? FLinearColor::White : FLinearColor(0.4f, 0.4f, 0.4f, 1.f)));
                }
                // For generic widgets we record edge ids in order and use a shared handler
                UButton* InnerBtn = Cast<UButton>(BtnWidget->GetWidgetFromName(TEXT("Choice_Button")));
                if (InnerBtn)
                {
                    PendingChoiceEdgeIds.Add(Choice.edgeId);
                    InnerBtn->OnClicked.AddDynamic(this, &UDialogueWidget::OnAnyChoiceClicked);
                    if (!Choice.conditionMet)
                    {
                        InnerBtn->SetIsEnabled(false);
                    }
                }
                Choices_Box->AddChild(BtnWidget);
            }
        }
        else
        {
            // Plain UDialogueChoiceButton fallback � create without a Blueprint subclass
            // This is lightweight and doesn't require a separate asset
            PendingChoiceEdgeIds.Add(Choice.edgeId);
            // We can't create UDialogueChoiceButton without a valid class pointer here,
            // so we fall back to storing the edge id and using the first-click heuristic
        }
    }
}

void UDialogueWidget::HandleChoiceButtonClicked(int32 EdgeId)
{
    OnChoiceClicked.Broadcast(EdgeId, true);
    if (DialogueManager)
    {
        DialogueManager->SendChoice(EdgeId);
    }
}

void UDialogueWidget::OnAnyChoiceClicked()
{
    // This fires for generic inner buttons; we use the first EdgeId in PendingChoiceEdgeIds
    // that corresponds to an enabled button. For multi-choice scenarios, prefer the
    // UDialogueChoiceButton path which carries the EdgeId directly.
    if (!Choices_Box || PendingChoiceEdgeIds.IsEmpty())
    {
        return;
    }

    // Walk children in order to match PendingChoiceEdgeIds[i]
    int32 EnabledIndex = 0;
    for (int32 i = 0; i < Choices_Box->GetChildrenCount(); ++i)
    {
        UWidget* Child = Choices_Box->GetChildAt(i);
        UButton* Btn = nullptr;
        UUserWidget* W = Cast<UUserWidget>(Child);
        if (W) Btn = Cast<UButton>(W->GetWidgetFromName(TEXT("Choice_Button")));

        if (Btn && Btn->GetIsEnabled())
        {
            if (EnabledIndex < PendingChoiceEdgeIds.Num())
            {
                HandleChoiceButtonClicked(PendingChoiceEdgeIds[EnabledIndex]);
                return;
            }
        }
        if (Btn) ++EnabledIndex;
    }

    // Absolute fallback
    if (PendingChoiceEdgeIds.Num() > 0)
    {
        HandleChoiceButtonClicked(PendingChoiceEdgeIds[0]);
    }
}

// ??? Button & delegate handlers ???????????????????????????????????????????????

void UDialogueWidget::HandleCloseButtonClicked()
{
    if (DialogueManager)
    {
        DialogueManager->CloseDialogue();
    }
    HideDialogue();
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

FReply UDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldStartDrag = false;

        if (DragHandle)
        {
            const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
            const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
            bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
                                LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);
        }
        else
        {
            bShouldStartDrag = true;
        }

        if (bShouldStartDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            const FVector2D MouseVP = InMouseEvent.GetScreenSpacePosition() / Scale;
            DragOffset = MouseVP - CurrentViewportPosition;
            bDragging = true;

            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            {
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            }
            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

FReply UDialogueWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
        {
            return FReply::Handled().ReleaseMouseCapture();
        }
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply UDialogueWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void UDialogueWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);

    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D ViewportSize = FVector2D(W, H) / Scale;

    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 300);

    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, ViewportSize.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - Size.Y));

    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

void UDialogueWidget::HandleDialogueNodeReceived(const FDialogueNodeData& NodeData)
{
    ShowDialogueNode(NodeData);
}

void UDialogueWidget::HandleDialogueSessionClosed(const FString& /*SessionId*/)
{
    HideDialogue();
}

void UDialogueWidget::HandleDialogueError(const FDialogueErrorData& /*ErrorData*/)
{
    // Server errors (e.g. OUT_OF_RANGE) are not dialogue content.
    // If a session was never started, the widget should stay hidden.
    // If a session was already open, close it cleanly.
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        HideDialogue();
    }
}
