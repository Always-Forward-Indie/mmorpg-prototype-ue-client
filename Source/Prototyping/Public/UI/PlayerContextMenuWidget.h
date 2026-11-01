#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Utils/NetCore/PlayerSocialRule.h"
#include "PlayerContextMenuWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerSocialActionSelected,
	EPlayerSocialAction, Action, int32, TargetCharacterId);

/**
 * Unit context menu (WoW-style right-click on a player): Whisper / Trade /
 * Inspect. Two interaction models:
 *
 * Code fallback (default): rows are built in C++ (Border + TextBlock) and
 * hit-tested by slate geometry with mouse capture, so the RMB release that
 * opened the menu never leaks through.
 *
 * Blueprint layout (optional): create a BP subclass, bind these names,
 * assign it to UIManager.PlayerMenuWidgetClass. Real buttons replace the
 * hit-testing (no mouse capture in BP mode); unavailable actions hide:
 *   Menu_Title_Text   UTextBlock  "Name [Level]" header
 *   Menu_Whisper_Btn  UButton     whisper (always shown)
 *   Menu_Trade_Btn    UButton     trade (hidden for invalid targets)
 *   Menu_Inspect_Btn  UButton     inspect (hidden when not allowed)
 * Bind all four or none.
 *
 * Created at runtime by UUIManager (viewport Z-order 1100).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerContextMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPlayerContextMenuWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Player Menu")
	void ShowForPlayer(int32 TargetCharacterId, const FString& DisplayName, FVector2D ScreenPosition, bool bCanInspect = true);

	UFUNCTION(BlueprintCallable, Category = "Player Menu")
	void HideMenu();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Menu")
	bool IsMenuOpen() const;

	UPROPERTY(BlueprintAssignable, Category = "Player Menu")
	FOnPlayerSocialActionSelected OnActionSelected;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Player Menu")
	int32 TargetCharacterId = 0;

	// Blueprint layout slots (BindWidgetOptional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Menu_Title_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Menu_Whisper_Btn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Menu_Trade_Btn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Menu_Inspect_Btn = nullptr;

protected:
	UFUNCTION() void HandleWhisperClicked();
	UFUNCTION() void HandleTradeClicked();
	UFUNCTION() void HandleInspectClicked();

private:
	struct FMenuRow
	{
		EPlayerSocialAction Action;
		UWidget* Widget = nullptr;
	};
	TArray<FMenuRow> Rows;
	int32 PressedRowIndex = -1;

	void RebuildRows(const FString& DisplayName, bool bCanTrade, bool bCanInspect);
	void AddRow(EPlayerSocialAction Action, const FText& Label);
	int32 HitTestRowIndex(const FVector2D& AbsoluteScreenPos) const;

	void FireAction(EPlayerSocialAction Action);

	// True when a BP subclass provides buttons (no mouse capture then).
	bool bUsingBpLayout = false;
	bool bWired = false;
};
