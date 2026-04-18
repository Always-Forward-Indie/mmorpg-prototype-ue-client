#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/WIODataStructs.h"
#include "WIOInteractionPromptWidget.generated.h"

class AWorldInteractiveObjectActor;
class UTextBlock;
class UImage;

/**
 * UWIOInteractionPromptWidget
 *
 * HUD widget shown when the local player is inside a WIO's interaction sphere.
 * C++ handles all text updates automatically — no Blueprint logic required.
 *
 * In your WBP, add these widgets with EXACT names:
 *   - TextBlock  "ObjectNameText"        — filled with the localised object name
 *   - TextBlock  "InteractionPromptText" — filled with e.g. "[F] Examine"
 *   - Image      "InteractionIcon"       — optional, set by C++ if a texture is found
 *
 * All three are BindWidgetOptional, so missing them causes no crash.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UWIOInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Show the prompt for a specific WIO actor. Fully handled in C++. */
	UFUNCTION(BlueprintCallable, Category = "WIO UI")
	void ShowForObject(AWorldInteractiveObjectActor* InActor);

	/** Hide the prompt. */
	UFUNCTION(BlueprintCallable, Category = "WIO UI")
	void HidePrompt();

	/** Returns the currently displayed WIO actor, if any. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO UI")
	AWorldInteractiveObjectActor* GetCurrentActor() const { return CurrentActor.Get(); }

	/** Returns true if the prompt is currently visible. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO UI")
	bool IsPromptVisible() const { return bIsVisible; }

	/** Key name shown in the prompt (default "F"). Editable per-blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO UI|Config")
	FString InteractionKeyName = TEXT("F");

protected:
	virtual void NativeConstruct() override;

	// ─── Auto-bound widget members ────────────────────────────────────────
	// Name these exactly in your WBP canvas.

	/** Displays the localised object name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "WIO UI")
	UTextBlock* ObjectNameText = nullptr;

	/** Displays the interaction verb, e.g. "[F] Examine". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "WIO UI")
	UTextBlock* InteractionPromptText = nullptr;

	/** Optional icon image. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "WIO UI")
	UImage* InteractionIcon = nullptr;

	/**
	 * Optional BP hook called AFTER C++ has already updated all text/icon.
	 * Implement for extra animations or sound — not required for basic display.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO UI")
	void OnPromptUpdated(const FWorldObjectData& ObjectData, const FText& DisplayName, const FText& PromptText);

private:
	TWeakObjectPtr<AWorldInteractiveObjectActor> CurrentActor;
	bool bIsVisible = false;
};
