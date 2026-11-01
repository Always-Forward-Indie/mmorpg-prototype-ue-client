#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Utils/NetCore/PlayerSocialRule.h"
#include "PlayerContextMenuWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerSocialActionSelected,
	EPlayerSocialAction, Action, int32, TargetCharacterId);

/**
 * Unit context menu (WoW-style right-click on a player): Whisper / Trade /
 * Inspect. Single mouse-capture target like UItemActionMenuWidget: LMB
 * press+release on the same row fires, every other button is consumed so
 * the RMB release that opened the menu never leaks through.
 *
 * Created at runtime by UUIManager (viewport Z-order 1100) — no BP setup.
 * Rows are built in C++ (Border + TextBlock); styling is functional,
 * a BP subclass may override rows later via ActionRowWidgetClass.
 */
UCLASS()
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
};
