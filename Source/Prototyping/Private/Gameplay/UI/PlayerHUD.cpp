// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/PlayerHUD.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerHUD::SetHP(float NewHP)
{
    if (HealthBar)
    {
        HealthBar->SetPercent(NewHP);
    }
}

void UPlayerHUD::SetMana(float NewMana)
{
    if (ManaBar)
    {
        ManaBar->SetPercent(NewMana);
    }
}

void UPlayerHUD::SetXP(float NewXP)
{
    if (XPBar)
    {
        XPBar->SetPercent(NewXP);
    }
}

void UPlayerHUD::SetLevel(int NewLevel)
{
    if (LevelText)
    {
        LevelText->SetText(FText::AsNumber(NewLevel));
    }
}