#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FocusableWindowWidget.generated.h"

/**
 * Delegate broadcast when the user clicks anywhere on this window.
 * UIManager listens to this to bring the window to front (Z-Order).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWindowFocusRequested, UUserWidget*, Widget);

/**
 * Base class for all draggable / focusable popup windows.
 * Any mouse-button-down on the widget broadcasts OnWindowFocusRequested so
 * UIManager can call BringWindowToFront().
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UFocusableWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Broadcast when the user presses any mouse button on this window. */
	UPROPERTY(BlueprintAssignable, Category = "Window Focus")
	FOnWindowFocusRequested OnWindowFocusRequested;

protected:
// NativeOnPreviewMouseButtonDown is called before child widgets handle the event,
// guaranteed to fire even when the subclass returns FReply::Handled() without Super.
virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
