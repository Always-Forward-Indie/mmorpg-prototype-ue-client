#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Gameplay/Input/KeybindManager.h"
#include "KeybindWidget.generated.h"

class ABasicPlayer;
class UScrollBox;
class UVerticalBox;
class UButton;
class UTextBlock;
class UKeybindWidget;

/**
 * Row click binding (defined first so UKeybindWidget can hold TArray of it).
 */
UCLASS()
class PROTOTYPING_API UKeybindRowBinding : public UObject
{
	GENERATED_BODY()

public:
	void Setup(UKeybindWidget* InWidget, UInputAction* InAction);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY() UKeybindWidget* Widget = nullptr;
	UPROPERTY() UInputAction* Action = nullptr;
};

/**
 * UKeybindWidget
 *
 * Controls list: every remappable action with its primary key,
 * click-to-remap (next keydown wins, Esc cancels), Reset all button.
 *
 * Two homes: standalone window (created in code by UIManager) or embedded
 * "Controls" panel inside WBP_Settings (slot 3 of TabContentSwitcher —
 * see UW_SettingsWidget class doc). The embedded BP instance sets
 * bManageViewportPlacement=false; its Close button is simply not bound
 * (settings has its own Button_Close).
 *
 * Blueprint layout (optional): create a BP subclass, bind these names.
 * Bind all three or none — rows are always built in code (uniform styling):
 *   Keybinds_List_Box   UScrollBox  action rows go here
 *   Keybinds_Reset_Btn  UButton     "Reset all" (restores asset defaults)
 *   Keybinds_Close_Btn  UButton     hides the window (standalone only)
 * With no subclass assigned the whole layout is built in C++ (headless-safe).
 *
 * Overrides persist in Game.ini via UKeybindManager and re-apply on every
 * input init.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UKeybindWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void BindToPlayer(ABasicPlayer* InPlayer, UKeybindManager* InManager);

	/** Rebuild the action list (call after BindToPlayer or tab switch). */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void RefreshList();

	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void CloseKeybinds();

	/** Abort an in-progress key capture without hiding (settings tab close). */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void CancelCapture();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keybinds")
	bool IsKeybindsOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	FVector2D WindowSize = FVector2D(440.f, 520.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	FVector2D WindowPosition = FVector2D(0.f, 0.f);

	/**
	 * Standalone window vs embedded settings-tab panel. The settings BP
	 * instance sets this false (viewport placement is meaningless inside
	 * a WidgetSwitcher slot).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	bool bManageViewportPlacement = true;

	// Blueprint layout slots (BindWidgetOptional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* Keybinds_List_Box = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Keybinds_Reset_Btn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Keybinds_Close_Btn = nullptr;

private:
	void BuildLayout();
	void ClearRows();
	void AddActionRow(const FKeybindActionEntry& Entry, const FKey& BoundKey);

public:
	/** Begin key capture for an action (invoked by row bindings). */
	void RequestCapture(UInputAction* Action);

private:
	void StopCapture();
	void OnCaptureKeyDown(const FKeyEvent& InKeyEvent);
	void HandleResetAllClicked();
	void HandleCloseClicked();

	UPROPERTY() ABasicPlayer* Player = nullptr;
	UPROPERTY() UKeybindManager* KeybindManager = nullptr;
	UPROPERTY() UInputMappingContext* MappingContext = nullptr;

	UPROPERTY() UScrollBox* ListBox = nullptr;
	UPROPERTY() UVerticalBox* RootCol = nullptr;
	UPROPERTY() TArray<UKeybindRowBinding*> RowBindings;

	UInputAction* CapturingAction = nullptr;
	FDelegateHandle CaptureHandle;

	// Wiring happens once (NativeConstruct may re-run on re-add to viewport).
	bool bWired = false;
};
