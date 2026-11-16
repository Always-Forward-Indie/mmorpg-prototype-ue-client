#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/ItemActionMenuWidget.h"
#include "ItemActionRowWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionRowClicked, EItemContextAction, Action);

/**
 * A single action row inside the item context menu.
 * Subclass this in Blueprint, bind the named widgets, and assign
 * ActionRowWidgetClass in ItemActionMenuWidget details to customise
 * the appearance of every action row (icon, background, font, etc.).
 *
 * Input: rows with a bound 'RowButton' fire through it (native hover,
 * press, focus, click sounds); rows without one stay purely visual and
 * the menu-level capture path (HitTestRowIndex) handles them as before.
 */
UCLASS(Blueprintable)
class PROTOTYPING_API UItemActionRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── Public API ──────────────────────────────────────────────────────────

	/** Set the label text displayed on this row. */
	void SetActionLabel(const FText& InLabel);

	/** Set an optional icon displayed on this row. */
	UFUNCTION(BlueprintCallable, Category = "Item Action Row")
	void SetActionIcon(UTexture2D* InIcon);

	/** The context action this row represents. */
	EItemContextAction GetBoundAction() const { return BoundAction; }
	void SetBoundAction(EItemContextAction InAction) { BoundAction = InAction; }

	/** Fired when the row's own button is clicked (no-op without RowButton). */
	UPROPERTY(BlueprintAssignable, Category = "Item Action Row Events")
	FOnActionRowClicked OnRowClicked;

	/** Code-built Button+Text content (no BP). Called by the menu. */
	void BuildCodeRow(const FText& Label);

	/** Hit-test in absolute screen coordinates. */
	bool IsPointInside(const FVector2D& AbsoluteScreenPos) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// ── Appearance ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor NormalBackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor HoverBackgroundColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.95f);

	// ── Bind Widgets (override in Blueprint) ────────────────────────────────

	// Single input path when bound (fills the row): native hover/press/focus.
	// Absent = visual-only row, the menu capture path handles the click.
	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* RowButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* RowBackground;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ActionLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ActionIcon;

private:
	UFUNCTION()
	void HandleRowButtonClicked();

	void WireButton();

	EItemContextAction BoundAction;
};
