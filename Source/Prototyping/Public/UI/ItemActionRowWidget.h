#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/ItemActionMenuWidget.h"
#include "ItemActionRowWidget.generated.h"

/**
 * A single action row inside the item context menu.
 * Subclass this in Blueprint, bind the named widgets, and assign
 * ActionRowWidgetClass in ItemActionMenuWidget details to customise
 * the appearance of every action row (icon, background, font, etc.).
 *
 * Input handling stays in ItemActionMenuWidget (HitTestRowIndex +
 * capture logic) so rows are purely visual containers with data.
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

	/** Hit-test in absolute screen coordinates. */
	bool IsPointInside(const FVector2D& AbsoluteScreenPos) const;

protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// ── Appearance ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor NormalBackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor HoverBackgroundColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.95f);

	// ── Bind Widgets (override in Blueprint) ────────────────────────────────

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* RowBackground;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ActionLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ActionIcon;

private:
	EItemContextAction BoundAction;
};
