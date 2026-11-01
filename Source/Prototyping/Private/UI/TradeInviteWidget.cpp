#include "UI/TradeInviteWidget.h"
#include "Prototyping.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

#define LOCTEXT_NAMESPACE "TradeInvite"

void UTradeInviteWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Blueprint layout wins when bound; otherwise build the code fallback.
	// Widget-level placement stays in code in both modes (viewport anchors
	// are runtime-only, a BP layout cannot provide them).
	if (!bWired)
	{
		bWired = true;
		if (Invite_Name_Text || Invite_Countdown_Text || Invite_Accept_Btn || Invite_Decline_Btn)
		{
			TitleText = Invite_Name_Text;
			CountdownText = Invite_Countdown_Text;
			AcceptBtn = Invite_Accept_Btn;
			DeclineBtn = Invite_Decline_Btn;
			if (AcceptBtn) AcceptBtn->OnClicked.AddDynamic(this, &UTradeInviteWidget::HandleAcceptClicked);
			if (DeclineBtn) DeclineBtn->OnClicked.AddDynamic(this, &UTradeInviteWidget::HandleDeclineClicked);
		}
		else
		{
			BuildLayout(); // wires its own buttons
		}
	}
	SetAnchorsInViewport(FAnchors(0.5f, 0.f));
	SetAlignmentInViewport(FVector2D(0.5f, 0.f));
	SetPositionInViewport(FVector2D(0.f, 80.f), false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTradeInviteWidget::NativeDestruct()
{
	if (TradeManager)
	{
		TradeManager->OnTradeInviteReceivedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteReceived);
		TradeManager->OnTradeStateUpdatedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleSessionOpened);
		TradeManager->OnTradeDeclinedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteDeclined);
		TradeManager->OnTradeCancelledDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteCancelled);
		TradeManager->OnTradeCompletedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteCompleted);
	}
	TradeManager = nullptr;
	Super::NativeDestruct();
}

void UTradeInviteWidget::BuildLayout()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree || TitleText) return;

	USizeBox* Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(320.f);
	Root->SetHeightOverride(140.f);
	Tree->RootWidget = Root;

	UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.96f));
	Frame->SetPadding(FMargin(14.f, 10.f));
	Root->AddChild(Frame);

	UVerticalBox* Col = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->AddChild(Col);

	TitleText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
	TitleText->SetShadowColorAndOpacity(FLinearColor::Black);
	TitleText->SetShadowOffset(FVector2D(1.f, 1.f));
	TitleText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* Head = Cast<UVerticalBoxSlot>(Col->AddChildToVerticalBox(TitleText)))
		Head->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	CountdownText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CountdownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	if (UVerticalBoxSlot* Count = Cast<UVerticalBoxSlot>(Col->AddChildToVerticalBox(CountdownText)))
		Count->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col->AddChildToVerticalBox(Row);

	AcceptBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* AcceptTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	AcceptTxt->SetText(LOCTEXT("Accept", "Accept"));
	AcceptBtn->AddChild(AcceptTxt);
	AcceptBtn->OnClicked.AddDynamic(this, &UTradeInviteWidget::HandleAcceptClicked);
	if (UHorizontalBoxSlot* A = Cast<UHorizontalBoxSlot>(Row->AddChildToHorizontalBox(AcceptBtn)))
		A->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

	DeclineBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* DeclineTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DeclineTxt->SetText(LOCTEXT("Decline", "Decline"));
	DeclineBtn->AddChild(DeclineTxt);
	DeclineBtn->OnClicked.AddDynamic(this, &UTradeInviteWidget::HandleDeclineClicked);
	Row->AddChildToHorizontalBox(DeclineBtn);
}

void UTradeInviteWidget::BindToTradeManager(UTradeManager* InTradeManager)
{
	if (TradeManager)
	{
		TradeManager->OnTradeInviteReceivedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteReceived);
		TradeManager->OnTradeStateUpdatedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleSessionOpened);
		TradeManager->OnTradeDeclinedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteDeclined);
		TradeManager->OnTradeCancelledDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteCancelled);
		TradeManager->OnTradeCompletedDelegate.RemoveDynamic(this, &UTradeInviteWidget::HandleInviteCompleted);
	}
	TradeManager = InTradeManager;
	if (!TradeManager) return;
	TradeManager->OnTradeInviteReceivedDelegate.AddDynamic(this, &UTradeInviteWidget::HandleInviteReceived);
	TradeManager->OnTradeStateUpdatedDelegate.AddDynamic(this, &UTradeInviteWidget::HandleSessionOpened);
	TradeManager->OnTradeDeclinedDelegate.AddDynamic(this, &UTradeInviteWidget::HandleInviteDeclined);
	TradeManager->OnTradeCancelledDelegate.AddDynamic(this, &UTradeInviteWidget::HandleInviteCancelled);
	TradeManager->OnTradeCompletedDelegate.AddDynamic(this, &UTradeInviteWidget::HandleInviteCompleted);
}

void UTradeInviteWidget::HandleInviteReceived(const FTradeInviteData& Invite)
{
	// Never pop over an active session (defensive: the server forbids it).
	if (TradeManager && TradeManager->IsInTrade()) return;
	const FString From = !Invite.fromCharacterName.IsEmpty() ? Invite.fromCharacterName : Invite.initiatorName;
	ShowInvite(From);
}

void UTradeInviteWidget::HandleSessionOpened(const FTradeStateData& /*State*/)
{
	HideInvite();
}

void UTradeInviteWidget::HandleInviteDeclined(const FTradeDeclinedData& /*Data*/)
{
	HideInvite();
}

void UTradeInviteWidget::HandleInviteCancelled(const FTradeCancelledData& /*Data*/)
{
	HideInvite();
}

void UTradeInviteWidget::HandleInviteCompleted(const FTradeCompleteData& /*Data*/)
{
	HideInvite();
}

void UTradeInviteWidget::ShowInvite(const FString& FromName)
{
	if (TitleText)
		TitleText->SetText(FText::Format(LOCTEXT("WantsToTrade", "{0} wants to trade"),
			FText::FromString(FromName.IsEmpty() ? TEXT("?") : FromName)));
	InviteExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + InviteTimeoutSec : 0.0;
	bInviteOpen = true;
	SetVisibility(ESlateVisibility::Visible);
}

void UTradeInviteWidget::HideInvite()
{
	bInviteOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTradeInviteWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bInviteOpen || !GetWorld()) return;
	const float Left = static_cast<float>(InviteExpiresAt - GetWorld()->GetTimeSeconds());
	if (Left <= 0.f)
	{
		// Server TTL elapsed: decline so the session frees on both sides.
		// Runs even when the BP layout has no countdown text.
		HandleDeclineClicked();
		return;
	}
	if (CountdownText)
		CountdownText->SetText(FText::Format(LOCTEXT("ExpiresIn", "Expires in {0}s"), FText::AsNumber(FMath::CeilToInt(Left))));
}

void UTradeInviteWidget::HandleAcceptClicked()
{
	if (!TradeManager) { HideInvite(); return; }
	HideInvite(); // Dialog closes immediately; the session opens on tradeState.
	TradeManager->RespondToTradeInvite(TradeManager->GetPendingInvite().initiatorId, true);
}

void UTradeInviteWidget::HandleDeclineClicked()
{
	if (!TradeManager) { HideInvite(); return; }
	HideInvite();
	TradeManager->RespondToTradeInvite(TradeManager->GetPendingInvite().initiatorId, false);
}

#undef LOCTEXT_NAMESPACE
