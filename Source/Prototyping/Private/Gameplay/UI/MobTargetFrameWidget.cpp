#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/DataStructs.h"

void UMobTargetFrameWidget::SetMobInfo(const FString& MobSlug,
                                        const FString& MobName,
                                        int32 MobLevel,
                                        int32 CurrentHP, int32 MaxHP,
                                        bool bIsAggro,
                                        UTexture2D* Icon)
{
    // --- Portrait icon ---
    // Priority: 1) caller-supplied Icon (CachedIcon from BasicMOB, already in memory)
    //           2) DataTable lookup by MobSlug (async-safe, same pattern as BestiaryMobRowWidget)
    //           3) DefaultIcon fallback
    if (PortraitImage)
    {
        if (Icon)
        {
            // Fast path: caller already resolved the texture (BasicMOB::CachedIcon)
            ApplyPortraitTexture(Icon);
        }
        else if (!MobSlug.IsEmpty())
        {
            // Fetch from MobDefinitionTable — covers the case where CachedIcon is null
            // (e.g. mob was locked before SetupMobVisual finished, or Icon not set in DT)
            LoadPortraitFromTable(MobSlug);
        }
        else if (DefaultIcon)
        {
            ApplyPortraitTexture(DefaultIcon);
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
        MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("LVL: %d"), MobLevel)));
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

    // Reset portrait to default (or blank) so the stale icon is never shown for the next target
    if (PortraitImage)
    {
        if (DefaultIcon)
        {
            ApplyPortraitTexture(DefaultIcon);
        }
        else
        {
            PortraitImage->SetBrushFromTexture(nullptr);
        }
    }
}

void UMobTargetFrameWidget::ApplyPortraitTexture(UTexture2D* Texture)
{
    if (!PortraitImage || !Texture) return;
    PortraitImage->SetBrushFromTexture(Texture);
    PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMobTargetFrameWidget::LoadPortraitFromTable(const FString& MobSlug)
{
    UWorld* W = GetWorld();
    if (!W) return;

    UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance());
    if (!GI) return;

    UDataTable* DT = GI->GetMobDefinitionTable();
    if (!DT) return;

    const FMobDefinition* Row = DT->FindRow<FMobDefinition>(FName(*MobSlug), TEXT(""));
    if (!Row)
    {
        // Slug not found — show default portrait if available
        if (DefaultIcon) { ApplyPortraitTexture(DefaultIcon); }
        return;
    }

    const TSoftObjectPtr<UTexture2D>& IconPtr = Row->Visual.Icon;
    if (IconPtr.IsNull())
    {
        if (DefaultIcon) { ApplyPortraitTexture(DefaultIcon); }
        return;
    }

    // Synchronous hot path: texture is already resident in memory
    if (UTexture2D* Already = IconPtr.Get())
    {
        ApplyPortraitTexture(Already);
        return;
    }

    // Async path: stream the texture without blocking the game thread
    TWeakObjectPtr<UMobTargetFrameWidget> WeakThis(this);
    TSoftObjectPtr<UTexture2D> IconSoft = IconPtr;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        IconSoft.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, IconSoft]()
        {
            if (!WeakThis.IsValid()) return;
            if (UTexture2D* Loaded = IconSoft.Get())
            {
                WeakThis->ApplyPortraitTexture(Loaded);
            }
        })
    );
}
