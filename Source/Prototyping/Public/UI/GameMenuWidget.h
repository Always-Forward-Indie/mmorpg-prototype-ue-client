// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMenuWidget.generated.h"

class UButton;
class UMyGameInstance;

// Delegates � business logic lives in UIManager; we only broadcast events.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameMenuResumeClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameMenuSettingsClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameMenuExitToLoginClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameMenuExitToDesktopClicked);

/**
 * UGameMenuWidget
 *
 * Pause / main game menu shown when the player presses Escape (or the menu hotkey).
 * Purely a presenter � business logic lives in UIManager.
 *
 * Required UMG widget names (BindWidget):
 *   Btn_Resume    — closes this menu and returns to game
 *   Btn_Settings  — opens the full tabbed settings window
 *
 * Optional UMG widget names (BindWidgetOptional):
 *   Btn_ExitToLogin     — disconnects and returns to the login screen
 *   Btn_ExitToDesktop   — quits the application
 *
 * Create a Blueprint child (e.g. WBP_GameMenu) and lay out those buttons.
 * Bind OnResume / OnSettingsClicked / OnExitToLogin / OnExitToDesktop in UIManager.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Delegates (broadcast on button click)
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Game Menu|Events")
	FOnGameMenuResumeClicked OnResumeClicked;

	UPROPERTY(BlueprintAssignable, Category = "Game Menu|Events")
	FOnGameMenuSettingsClicked OnSettingsClicked;

	UPROPERTY(BlueprintAssignable, Category = "Game Menu|Events")
	FOnGameMenuExitToLoginClicked OnExitToLoginClicked;

	UPROPERTY(BlueprintAssignable, Category = "Game Menu|Events")
	FOnGameMenuExitToDesktopClicked OnExitToDesktopClicked;

	// -----------------------------------------------------------------------
	// UMG bindings
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Game Menu")
	UButton* Btn_Resume = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Game Menu")
	UButton* Btn_Settings = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu")
	UButton* Btn_ExitToLogin = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu")
	UButton* Btn_ExitToDesktop = nullptr;

	// -----------------------------------------------------------------------
	// API
	// -----------------------------------------------------------------------

	/** Show this widget and pause input routing to the game. */
	UFUNCTION(BlueprintCallable, Category = "Game Menu")
	void OpenMenu();

	/** Hide this widget and return control to the game. */
	UFUNCTION(BlueprintCallable, Category = "Game Menu")
	void CloseMenu();

	/** Toggle open/close state. */
	UFUNCTION(BlueprintCallable, Category = "Game Menu")
	void ToggleMenu();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Menu")
	bool IsMenuOpen() const { return bIsOpen; }

protected:
	virtual void NativeConstruct() override;

private:
bool bIsOpen = false;
bool bDelegatesBound = false;

UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();
	UFUNCTION()
	void HandleExitToLoginClicked();

	UFUNCTION()
	void HandleExitToDesktopClicked();
};
