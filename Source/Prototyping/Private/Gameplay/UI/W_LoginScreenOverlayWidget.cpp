// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/W_LoginScreenOverlayWidget.h"
#include "MyGameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

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
