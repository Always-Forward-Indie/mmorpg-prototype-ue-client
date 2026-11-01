#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "TradeInviteWidget.generated.h"

class UTradeManager;

/**
 * UTradeInviteWidget
 *
 * Standalone trade-invite dialog (WoW-style static popup): inviter name,
 * Accept / Decline, countdown to the server session TTL with auto-decline.
 * Self-contained: subscribes to UTradeManager invites + terminal events.
 * Layout is built in C++ — no Blueprint setup required.
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

	class UTextBlock* TitleText = nullptr;
	class UTextBlock* CountdownText = nullptr;
	class UButton* AcceptBtn = nullptr;
	class UButton* DeclineBtn = nullptr;

	bool bInviteOpen = false;
	double InviteExpiresAt = 0.0;
};
