// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/PlayerHUD.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerHUD::NativeConstruct()
{
    Super::NativeConstruct();
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerHUD: Constructed"));
}

void UPlayerHUD::NativeDestruct()
{
    Super::NativeDestruct();
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerHUD: Destructed"));
}

void UPlayerHUD::SetHP(float NewHP, float MaxHP)
{
    if (HealthBar)
    {
        // Calculate percentage for progress bar
        float Percentage = (MaxHP > 0.0f) ? (NewHP / MaxHP) : 0.0f;
        HealthBar->SetPercent(Percentage);
    }

    if (HealthBarTextValue)
    {
        // Format text as "current/max"
        FString HPText = FString::Printf(TEXT("%.0f/%.0f"), NewHP, MaxHP);
        HealthBarTextValue->SetText(FText::FromString(HPText));
    }
}

void UPlayerHUD::SetMana(float NewMana, float MaxMana)
{
    if (ManaBar)
    {
        // Calculate percentage for progress bar
        float Percentage = (MaxMana > 0.0f) ? (NewMana / MaxMana) : 0.0f;
        ManaBar->SetPercent(Percentage);
    }

    if (ManaBarTextValue)
    {
        // Format text as "current/max"
        FString ManaText = FString::Printf(TEXT("%.0f/%.0f"), NewMana, MaxMana);
        ManaBarTextValue->SetText(FText::FromString(ManaText));
    }
}

bool UPlayerHUD::IsHUDReady() const
{
    return IsValid(HealthBar) && IsValid(ManaBar) && 
           IsValid(HealthBarTextValue) && IsValid(ManaBarTextValue);
}