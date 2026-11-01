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
 * Runtime-built controls list: every remappable action with its primary key,
 * click-to-remap (next keydown wins, Esc cancels), Reset all button.
 * No Blueprint setup required. Overrides persist in Game.ini via
 * UKeybindManager and re-apply on every input init.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UKeybindWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void BindToPlayer(ABasicPlayer* InPlayer, UKeybindManager* InManager);

	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void RefreshList();

	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void CloseKeybinds();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keybinds")
	bool IsKeybindsOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	FVector2D WindowSize = FVector2D(440.f, 520.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	FVector2D WindowPosition = FVector2D(0.f, 0.f);

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
};
