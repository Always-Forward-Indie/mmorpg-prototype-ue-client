#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UMobTargetFrameWidget::SetMobInfo(const FString& MobSlug,
                                        const FString& MobName,
                                        int32 MobLevel,
                                        int32 CurrentHP, int32 MaxHP,
                                        bool bIsAggro,
                                        UTexture2D* Icon)
{
    // Icon
    if (PortraitImage)
    {
        UTexture2D* Tex = Icon ? Icon : DefaultIcon;
        if (Tex)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(Tex);
            Brush.ImageSize = FVector2D(64.f, 64.f);
            PortraitImage->SetBrush(Brush);
        }
    }

    // Localized name — fall back to raw MobName if slug lookup fails
    if (MobNameText)
    {
        FText DisplayName;
        if (!MobSlug.IsEmpty())
        {
            if (UWorld* W = GetWorld())
            {
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance()))
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        DisplayName = Loc->GetMobDisplayName(MobSlug);
                    }
                }
            }
        }
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(MobName);
        }

        MobNameText->SetText(DisplayName);

        // Red for aggressive mobs, yellow for passive
        const FLinearColor NameColor = bIsAggro
            ? FLinearColor(1.f, 0.2f, 0.2f, 1.f)
            : FLinearColor(1.f, 0.85f, 0.f, 1.f);
        MobNameText->SetColorAndOpacity(FSlateColor(NameColor));
    }

    // Level
    if (MobLevelText)
    {
        MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv. %d"), MobLevel)));
    }

    // Aggro icon
    if (AggroIcon)
    {
        AggroIcon->SetVisibility(bIsAggro ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    UpdateHP(CurrentHP, MaxHP);

    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMobTargetFrameWidget::UpdateHP(int32 CurrentHP, int32 MaxHP)
{
    if (MobHealthBar && MaxHP > 0)
    {
        MobHealthBar->SetPercent(static_cast<float>(CurrentHP) / static_cast<float>(MaxHP));
    }

    if (MobHealthText)
    {
        MobHealthText->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d"), FMath::Max(CurrentHP, 0), MaxHP)));
    }
}

void UMobTargetFrameWidget::ClearTarget()
{
    SetVisibility(ESlateVisibility::Collapsed);

    if (MobNameText)   MobNameText->SetText(FText::GetEmpty());
    if (MobLevelText)  MobLevelText->SetText(FText::GetEmpty());
    if (MobHealthBar)  MobHealthBar->SetPercent(0.f);
    if (MobHealthText) MobHealthText->SetText(FText::GetEmpty());
    if (AggroIcon)     AggroIcon->SetVisibility(ESlateVisibility::Collapsed);
}
