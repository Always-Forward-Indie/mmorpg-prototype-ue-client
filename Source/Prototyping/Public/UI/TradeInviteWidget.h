#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Data/DataStructs.h"
#include "TradeInviteWidget.generated.h"

class UTradeManager;

/**
 * UTradeInviteWidget
 *
 * Standalone trade-invite dialog (WoW-style static popup): inviter name,
 * Accept / Decline, countdown to the server session TTL with auto-decline.
 * Self-contained: subscribes to UTradeManager invites + terminal events.
 *
 * Blueprint layout (optional): create a BP subclass and bind these names
 * to replace the code-built fallback. Bind all four or none — a partially
 * bound layout still works (missing pieces are skipped), but the dialog
 * looks complete only with all four:
 *   Invite_Name_Text      UTextBlock  "X wants to trade"
 *   Invite_Countdown_Text UTextBlock  "Expires in Ns" (optional display;
 *                                     auto-decline works without it)
 *   Invite_Accept_Btn     UButton     accept the invite
 *   Invite_Decline_Btn    UButton     decline the invite
 * Assign the subclass to UIManager.TradeInviteWidgetClass. Dialog placement
 * comes from the BP root's own anchors (full-screen root + centered content
 * is the standard setup); code positions only the fallback. With no subclass
 * assigned the code-built fallback is used (headless-safe).
 *
 * Replaces the in-trade-window invite banner as the primary invite UI
 * (the banner stays as a fallback for the open-window case).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UTradeInviteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind the trade manager (subscribes to invite + terminal events). */
	UFUNCTION(BlueprintCallable, Category = "Trade Invite")
	void BindToTradeManager(UTradeManager* InTradeManager);

	/** Seconds until the server drops the pending invite (mirror of
	 *  the server TradeSession TTL — display only, server is authoritative). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Invite")
	float InviteTimeoutSec = 60.f;

	// Blueprint layout slots (BindWidgetOptional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Invite_Name_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Invite_Countdown_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Invite_Accept_Btn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Invite_Decline_Btn = nullptr;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleInviteReceived(const FTradeInviteData& Invite);
	UFUNCTION() void HandleSessionOpened(const FTradeStateData& State);
	UFUNCTION() void HandleInviteDeclined(const FTradeDeclinedData& Data);
	UFUNCTION() void HandleInviteCancelled(const FTradeCancelledData& Data);
	UFUNCTION() void HandleInviteCompleted(const FTradeCompleteData& Data);
	UFUNCTION() void HandleAcceptClicked();
	UFUNCTION() void HandleDeclineClicked();

private:
	void BuildLayout();
	void ShowInvite(const FString& FromName);
	void HideInvite();

	UPROPERTY() UTradeManager* TradeManager = nullptr;

	// Active widgets: Blueprint-bound when a BP subclass provides them,
	// otherwise the code-built fallback. Never touch Invite_*_ directly.
	class UTextBlock* TitleText = nullptr;
	class UTextBlock* CountdownText = nullptr;
	class UButton* AcceptBtn = nullptr;
	class UButton* DeclineBtn = nullptr;

	// Wiring happens once (NativeConstruct may re-run on re-add to viewport).
	bool bWired = false;
	bool bInviteOpen = false;
	double InviteExpiresAt = 0.0;
};
