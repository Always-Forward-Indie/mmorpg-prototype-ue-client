// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/W_LoginScreenOverlayWidget.h"
#include "MyGameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformProcess.h"

void UW_LoginScreenOverlayWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SettingsButton)
    {
        SettingsButton->OnClicked.AddDynamic(this, &UW_LoginScreenOverlayWidget::HandleSettingsClicked);
    }

    if (ExitGameButton)
    {
        ExitGameButton->OnClicked.AddDynamic(this, &UW_LoginScreenOverlayWidget::HandleExitGameClicked);
    }

    if (BugReportButton)
    {
        BugReportButton->OnClicked.AddDynamic(this, &UW_LoginScreenOverlayWidget::HandleBugReportClicked);
    }
}

void UW_LoginScreenOverlayWidget::HandleSettingsClicked()
{
    OnSettingsClicked();
}

void UW_LoginScreenOverlayWidget::HandleExitGameClicked()
{
    OnExitGameClicked();
}

void UW_LoginScreenOverlayWidget::OnSettingsClicked_Implementation()
{
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        GI->ShowLoginSettings();
    }
}

void UW_LoginScreenOverlayWidget::OnExitGameClicked_Implementation()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UW_LoginScreenOverlayWidget::HandleBugReportClicked()
{
    if (!BugReportUrl.IsEmpty())
    {
        FPlatformProcess::LaunchURL(*BugReportUrl, nullptr, nullptr);
    }
}
