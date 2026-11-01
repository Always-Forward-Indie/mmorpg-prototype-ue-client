#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Utils/NetCore/PlayerSocialRule.h"
#include "PlayerMenuRowWidget.generated.h"

/**
 * A single action row inside the player unit menu.
 * Subclass this in Blueprint, bind MenuRow_Label, and assign
 * MenuRowWidgetClass in UPlayerContextMenuWidget details to customise
 * the appearance of every menu row.
 *
 * Input handling stays in UPlayerContextMenuWidget (HitTestRowIndex +
 * capture logic) so rows are purely visual containers with data.
 */
UCLASS(Blueprintable)
class PROTOTYPING_API UPlayerMenuRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set the label text displayed on this row. */
	void SetActionLabel(const FText& InLabel);

	/** The social action this row represents. */
	EPlayerSocialAction GetBoundAction() const { return BoundAction; }
	void SetBoundAction(EPlayerSocialAction InAction) { BoundAction = InAction; }

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
	class UBorder* RowBackground;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* MenuRow_Label;

private:
	EPlayerSocialAction BoundAction = EPlayerSocialAction::Whisper;
};
