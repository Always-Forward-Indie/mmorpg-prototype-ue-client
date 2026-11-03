#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Utils/NetCore/PlayerSocialRule.h"
#include "PlayerContextMenuWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerSocialActionSelected,
	EPlayerSocialAction, Action, int32, TargetCharacterId);

class UPlayerMenuRowWidget;

/**
 * Unit context menu (WoW-style right-click on a player): Whisper / Trade /
 * Inspect. Rows follow the ItemActionMenu pattern: a bound VerticalBox
 * plus a row widget class, hit-tested by slate geometry with mouse capture,
 * so the RMB release that opened the menu never leaks through.
 *
 * Blueprint layout (optional): create a BP subclass and bind these names,
 * assign it to UIManager.PlayerMenuWidgetClass. Rows are built dynamically
 * (unavailable actions are skipped, never greyed out):
 *   Menu_Title_Text   UTextBlock   "Name [Level]" header (optional)
 *   Menu_Action_List  UVerticalBox rows go here (optional)
 *   MenuRowWidgetClass (EditAnywhere) row design; unset = code Border rows.
 *   Row BP binds MenuRow_Label (UTextBlock, required).
 * With nothing bound the whole menu is built in C++ (headless-safe).
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
	void ShowForPlayer(int32 TargetCharacterId, const FString& DisplayName, FVector2D ScreenPosition, bool bCanInspect = true, bool bCanTrade = true);

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

	// Blueprint layout (optional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Menu_Title_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UVerticalBox* Menu_Action_List = nullptr;

	/** Row widget class for BP-styled rows. Unset = code-built Border rows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Menu")
	TSubclassOf<UPlayerMenuRowWidget> MenuRowWidgetClass;

private:
	struct FMenuRow
	{
		EPlayerSocialAction Action;
		UWidget* Widget = nullptr;
	};
	TArray<FMenuRow> Rows;
	int32 PressedRowIndex = -1;

	void RebuildRows(const FString& DisplayName, bool bCanTrade, bool bCanInspect);
	void AddRow(EPlayerSocialAction Action, const FText& Label, UVerticalBox* List);
	int32 HitTestRowIndex(const FVector2D& AbsoluteScreenPos) const;

	void FireAction(EPlayerSocialAction Action);
};
