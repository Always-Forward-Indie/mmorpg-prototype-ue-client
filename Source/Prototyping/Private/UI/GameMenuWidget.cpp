// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/GameMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"

void UGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Guard: NativeConstruct can fire more than once if the widget is
	// removed and re-added.  Prevent duplicate button bindings.
	if (bDelegatesBound) { return; }
	bDelegatesBound = true;

	if (Btn_Resume)
		Btn_Resume->OnClicked.AddDynamic(this, &UGameMenuWidget::HandleResumeClicked);

	if (Btn_Settings)
		Btn_Settings->OnClicked.AddDynamic(this, &UGameMenuWidget::HandleSettingsClicked);

	if (Btn_ExitToLogin)
		Btn_ExitToLogin->OnClicked.AddDynamic(this, &UGameMenuWidget::HandleExitToLoginClicked);

	if (Btn_ExitToDesktop)
		Btn_ExitToDesktop->OnClicked.AddDynamic(this, &UGameMenuWidget::HandleExitToDesktopClicked);

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameMenuWidget::OpenMenu()
{
	if (bIsOpen) { return; }
	bIsOpen = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UGameMenuWidget::CloseMenu()
{
	if (!bIsOpen) { return; }
	bIsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameMenuWidget::ToggleMenu()
{
	bIsOpen ? CloseMenu() : OpenMenu();
}

void UGameMenuWidget::HandleResumeClicked()
{
	CloseMenu();
	OnResumeClicked.Broadcast();
}

void UGameMenuWidget::HandleSettingsClicked()
{
	OnSettingsClicked.Broadcast();
}

void UGameMenuWidget::HandleExitToLoginClicked()
{
	OnExitToLoginClicked.Broadcast();
}

void UGameMenuWidget::HandleExitToDesktopClicked()
{
	OnExitToDesktopClicked.Broadcast();
}
