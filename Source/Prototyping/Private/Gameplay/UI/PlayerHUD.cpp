#include "Gameplay/UI/PlayerHUD.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerHUD::NativeConstruct()
{
    Super::NativeConstruct();

    if (HealthBar)           { HealthBar->SetPercent(0.0f); }
    if (ManaBar)             { ManaBar->SetPercent(0.0f); }
    if (HealthBarTextValue)  { HealthBarTextValue->SetText(FText::FromString(TEXT("--/--"))); }
    if (ManaBarTextValue)    { ManaBarTextValue->SetText(FText::FromString(TEXT("--/--"))); }

    UE_LOG(LogTemp, Warning, TEXT("PlayerHUD: Constructed - bars reset to 0"));
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
        float Percentage = (MaxHP > 0.0f) ? (NewHP / MaxHP) : 0.0f;
        HealthBar->SetPercent(Percentage);
    }
    if (HealthBarTextValue)
    {
        HealthBarTextValue->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f/%.0f"), NewHP, MaxHP)));
    }
}

void UPlayerHUD::SetMana(float NewMana, float MaxMana)
{
    if (ManaBar)
    {
        float Percentage = (MaxMana > 0.0f) ? (NewMana / MaxMana) : 0.0f;
        ManaBar->SetPercent(Percentage);
    }
    if (ManaBarTextValue)
    {
        ManaBarTextValue->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f/%.0f"), NewMana, MaxMana)));
    }
}

bool UPlayerHUD::IsHUDReady() const
{
    return IsValid(HealthBar) && IsValid(ManaBar) &&
           IsValid(HealthBarTextValue) && IsValid(ManaBarTextValue);
}
