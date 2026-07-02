#include "Gameplay/UI/EffectSlotWidget.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UObject/SoftObjectPath.h"
#include "Services/LocalizationSubsystem.h"

void UEffectSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

// ?????????????????????????????????????????????????????????????????????????????
void UEffectSlotWidget::SetupSlot(const FActiveEffectEntry& InEffect, UDataTable* InTable)
{
    CachedEffect = InEffect;

    // Merge: prefer the explicitly passed table, fall back to the property
    UDataTable* Table = InTable ? InTable : EffectDefinitionTable.Get();

    bHasDefinition = false;
    CachedDefinition = FEffectDefinitionRow();

    if (Table)
    {
        const FEffectDefinitionRow* Row =
            Table->FindRow<FEffectDefinitionRow>(FName(*InEffect.slug), TEXT("EffectSlotWidget"));
        if (Row)
        {
            CachedDefinition = *Row;
            bHasDefinition = true;
        }
    }

    // Apply visuals
    ApplyIcon();
    ApplyBorderTint();
    RefreshTimer();

    // Build tooltip
    BuildTooltip();

    // Notify Blueprint
    OnSlotSetup(CachedEffect, CachedDefinition, bHasDefinition);
}

void UEffectSlotWidget::SetupSlotGrouped(const FActiveEffectEntry& InEffect,
                                          const TArray<FActiveEffectEntry>& InModifiers,
                                          UDataTable* InTable)
{
    CachedModifiers = InModifiers;
    SetupSlot(InEffect, InTable);
}





// ?????????????????????????????????????????????????????????????????????????????
void UEffectSlotWidget::RefreshTimer()
{
    if (Timer_Text)
    {
        const FString TimerStr = BuildTimerString();
        if (TimerStr.IsEmpty())
        {
            Timer_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            Timer_Text->SetVisibility(ESlateVisibility::HitTestInvisible);
            Timer_Text->SetText(FText::FromString(TimerStr));
        }
    }
}

// ?????????????????????????????????????????????????????????????????????????????
FString UEffectSlotWidget::BuildTimerString() const
{
    // Permanent effects (expiresAt == 0): no duration label shown.
    if (CachedEffect.expiresAt == 0)
    {
        return FString();
    }

    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();
    const int64 SecsLeft = FMath::Max<int64>(CachedEffect.expiresAt - NowSec, 0);

    if (SecsLeft >= 3600)
    {
        return FString::Printf(TEXT("%dh"), (int32)(SecsLeft / 3600));
    }
    if (SecsLeft >= 60)
    {
        return FString::Printf(TEXT("%dm"), (int32)(SecsLeft / 60));
    }
    return FString::Printf(TEXT("%ds"), (int32)SecsLeft);
}

FString UEffectSlotWidget::BuildModifiersString() const
{
    if (CachedModifiers.Num() == 0)
        return FString();

    FString Result;
    for (const FActiveEffectEntry& Mod : CachedModifiers)
    {
        if (!Mod.attributeSlug.IsEmpty())
        {
            const FString Sign = Mod.value >= 0.0f ? TEXT("+") : TEXT("");
            if (!Result.IsEmpty())
                Result += TEXT("\n");
            Result += FString::Printf(TEXT("%s: %s%.0f"), *Mod.attributeSlug, *Sign, Mod.value);
        }
    }
    return Result;
}



// ?????????????????????????????????????????????????????????????????????????????
void UEffectSlotWidget::ApplyIcon()
{
    if (!Effect_Icon)
        return;

    if (bHasDefinition && !CachedDefinition.Icon.IsNull())
    {
        // Synchronous load � icon assets are expected to be small and already loaded
        UTexture2D* IconTexture = CachedDefinition.Icon.LoadSynchronous();
        if (IconTexture)
        {
            Effect_Icon->SetBrushFromTexture(IconTexture, true);
            Effect_Icon->SetVisibility(ESlateVisibility::Visible);
            return;
        }
    }

    // No icon found � hide the image so the slot still renders cleanly
    Effect_Icon->SetVisibility(ESlateVisibility::Hidden);
}

// ?????????????????????????????????????????????????????????????????????????????
void UEffectSlotWidget::ApplyBorderTint()
{
    if (!Slot_Border)
        return;

    FLinearColor Tint = FLinearColor::White;

    if (bHasDefinition)
    {
        Tint = CachedDefinition.SlotTintColor;
    }
    else
    {
        // Fallback: derive rough tint from effectTypeSlug
        const FString& TypeSlug = CachedEffect.effectTypeSlug;
        if (TypeSlug.Contains(TEXT("debuff")) || TypeSlug.Contains(TEXT("dot")))
        {
            Tint = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); // reddish
        }
        else if (TypeSlug.Contains(TEXT("buff")) || TypeSlug.Contains(TEXT("hot")))
        {
            Tint = FLinearColor(0.3f, 1.0f, 0.4f, 1.0f); // greenish
        }
    }

    Slot_Border->SetBrushColor(Tint);
}

// ?????????????????????????????????????????????????????????????????????????????
void UEffectSlotWidget::BuildTooltip()
{
    if (EffectTooltipClass)
    {
        // Create a custom widget tooltip
        UUserWidget* TooltipWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), EffectTooltipClass);
        if (TooltipWidget)
        {
            if (UTextBlock* TitleText =
                    Cast<UTextBlock>(TooltipWidget->GetWidgetFromName(TEXT("Tooltip_Title"))))
            {
                FText Title;
                // 1. LocalizationSubsystem (per-language)
                if (UGameInstance* GI = GetGameInstance())
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        Title = Loc->GetEffectDisplayName(CachedEffect.slug);
                    }
                }
                // 2. Raw slug fallback
                if (Title.IsEmpty())
                    Title = FText::FromString(CachedEffect.slug);
                TitleText->SetText(Title);
            }

            if (UTextBlock* DescText =
                    Cast<UTextBlock>(TooltipWidget->GetWidgetFromName(TEXT("Tooltip_Description"))))
            {
                FString Desc;
                // 1. LocalizationSubsystem (per-language)
                if (UGameInstance* GI = GetGameInstance())
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        const FText LocDesc = Loc->GetEffectDescription(CachedEffect.slug);
                        if (!LocDesc.IsEmpty())
                            Desc = LocDesc.ToString();
                    }
                }

                const FString ModStr = BuildModifiersString();
                if (!ModStr.IsEmpty())
                {
                    if (!Desc.IsEmpty()) Desc += TEXT("\n");
                    Desc += ModStr;
                }

                if (Desc.IsEmpty())
                {
                    const FString TimerStr = BuildTimerString();
                    Desc = FString::Printf(TEXT("Type: %s\nDuration: %s"),
                        *CachedEffect.effectTypeSlug, *TimerStr);
                }

                DescText->SetText(FText::FromString(Desc));
            }

            SetToolTip(TooltipWidget);
        }
    }
    else
    {
        // Simple text tooltip as fallback
        FText Title;
        if (UGameInstance* GI = GetGameInstance())
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                Title = Loc->GetEffectDisplayName(CachedEffect.slug);
            }
        }
        if (Title.IsEmpty())
            Title = FText::FromString(CachedEffect.slug);

        FString DescStr;
        if (UGameInstance* GI = GetGameInstance())
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                const FText LocDesc = Loc->GetEffectDescription(CachedEffect.slug);
                if (!LocDesc.IsEmpty())
                    DescStr = LocDesc.ToString();
            }
        }

        const FString ModStr = BuildModifiersString();
        if (!ModStr.IsEmpty())
        {
            if (!DescStr.IsEmpty()) DescStr += TEXT("\n");
            DescStr += ModStr;
        }

        if (DescStr.IsEmpty())
        {
            DescStr = FString::Printf(TEXT("Type: %s  Duration: %s"),
                *CachedEffect.effectTypeSlug, *BuildTimerString());
        }

        SetToolTipText(FText::Format(
            FText::FromString(TEXT("{0}\n{1}")), Title, FText::FromString(DescStr)));
    }
}
