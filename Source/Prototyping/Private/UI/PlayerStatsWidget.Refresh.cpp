#include "UI/PlayerStatsWidget.h"
#include "Prototyping.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/MasteryManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/UserWidget.h"

// S5b: vitals labels gathered via LOCTEXT (same namespace as Titles TU).
#define LOCTEXT_NAMESPACE "PlayerStatsWidget"

void UPlayerStatsWidget::HandleStatsUpdated(const FPlayerStatsUpdateStruct& NewStats)
{
    CachedStats = NewStats;
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RefreshAll(NewStats);
        UpdateCountdownTimer();
    }
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::RefreshAll(const FPlayerStatsUpdateStruct& Stats)
{
    UE_LOG(LogGameplay, Verbose, TEXT("PlayerStatsWidget::RefreshAll - charId=%d lv=%d SP=%d effects=%d attrs=%d"),
        Stats.characterId, Stats.level, Stats.freeSkillPoints,
        Stats.activeEffects.Num(), Stats.attributes.Num());

    RefreshVitals(Stats);
    RefreshAttributes(Stats);
    RefreshEffects(Stats);
    RefreshEquippedTitle();
    RefreshMasteries();
}

void UPlayerStatsWidget::RefreshVitals(const FPlayerStatsUpdateStruct& Stats)
{
    // Level
    if (Level_Text)
        Level_Text->SetText(FText::Format(LOCTEXT("LevelValue", "Level {0}"), FText::AsNumber(Stats.level)));

    // Free skill points
    if (!SP_Text)
        UE_LOG(LogGameplay, Warning, TEXT("PlayerStatsWidget::RefreshVitals - SP_Text is NULL (add UTextBlock named 'SP_Text' to WBP_PlayerStats), SP value would be %d"), Stats.freeSkillPoints);
    if (SP_Text)
    {
        SP_Text->SetText(FText::Format(LOCTEXT("SkillPoints", "Skill Points: {0}"), FText::AsNumber(Stats.freeSkillPoints)));
        // Highlight in gold when there are unspent points, grey when zero
        SP_Text->SetColorAndOpacity(
            Stats.freeSkillPoints > 0
                ? FSlateColor(FLinearColor(1.0f, 0.85f, 0.1f))
                : FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
    }

    // HP
    if (HP_Bar)
    {
        const float Pct = Stats.healthMax > 0 ? (float)Stats.healthCurrent / (float)Stats.healthMax : 0.f;
        HP_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (HP_Text)
        HP_Text->SetText(FText::Format(LOCTEXT("HPValue", "HP: {0} / {1}"),
            FText::AsNumber(Stats.healthCurrent), FText::AsNumber(Stats.healthMax)));

    // MP
    if (MP_Bar)
    {
        const float Pct = Stats.manaMax > 0 ? (float)Stats.manaCurrent / (float)Stats.manaMax : 0.f;
        MP_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (MP_Text)
        MP_Text->SetText(FText::Format(LOCTEXT("MPValue", "MP: {0} / {1}"),
            FText::AsNumber(Stats.manaCurrent), FText::AsNumber(Stats.manaMax)));

    // XP bar: (current - levelStart) / (nextLevel - levelStart)
    if (XP_Bar)
    {
        const int32 Range = Stats.experienceNextLevel - Stats.experienceLevelStart;
        const int32 Delta = Stats.experienceCurrent  - Stats.experienceLevelStart;
        const float Pct   = Range > 0 ? (float)Delta / (float)Range : 0.f;
        XP_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (XP_Text)
        XP_Text->SetText(FText::Format(LOCTEXT("XPValue", "XP: {0} / {1}"),
            FText::AsNumber(Stats.experienceCurrent), FText::AsNumber(Stats.experienceNextLevel)));

    // Debt: show in red when non-zero, hide otherwise
    if (Debt_Text)
    {
        if (Stats.experienceDebt > 0)
        {
            Debt_Text->SetText(FText::Format(LOCTEXT("DebtValue", "Debt: {0} XP"), FText::AsNumber(Stats.experienceDebt)));
            Debt_Text->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Debt_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Weight
    if (Weight_Bar)
    {
        const float Pct = Stats.weightMax > 0.f ? Stats.weightCurrent / Stats.weightMax : 0.f;
        Weight_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (Weight_Text)
        Weight_Text->SetText(FText::Format(LOCTEXT("WeightValue", "Weight: {0} / {1} kg"),
            FText::FromString(FString::Printf(TEXT("%.1f"), Stats.weightCurrent)),
            FText::FromString(FString::Printf(TEXT("%.1f"), Stats.weightMax))));
}

void UPlayerStatsWidget::RefreshAttributes(const FPlayerStatsUpdateStruct& Stats)
{
    if (!Attributes_Box) return;
    Attributes_Box->ClearChildren();

    for (const FStatAttributeEntry& Attr : Stats.attributes)
    {
        const int32 EffInt = static_cast<int32>(Attr.effective);
        const int32 Bonus = EffInt - static_cast<int32>(Attr.base);

        // S3c: codebook table wins when it holds a real row; server name covers
        // unfilled tables/old servers; humanised slug is the last resort.
        FString Label = Attr.name;
        if (!Attr.slug.IsEmpty())
        {
            if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
                Label = ULocalizationSubsystem::PickTableOrServer(
                    Loc->GetAttributeDisplayName(Attr.slug), Attr.slug, Attr.name).ToString();
        }
        if (Label.IsEmpty())
            Label = JSONParser::HumanizeSlugForDisplay(Attr.slug);

        FString Line;
        if (Bonus > 0)
            Line = FString::Printf(TEXT("%s:  %d  (+%d)"), *Label, EffInt, Bonus);
        else if (Bonus < 0)
            Line = FString::Printf(TEXT("%s:  %d  (%d)"), *Label, EffInt, Bonus);
        else
            Line = FString::Printf(TEXT("%s:  %d"), *Label, EffInt);

        // Use a custom row widget if supplied; otherwise a plain TextBlock
        if (AttributeRowClass)
        {
            UUserWidget* RowWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), AttributeRowClass);
            if (RowWidget)
            {
                // The row widget is expected to expose a TextBlock named "Row_Text"
                UTextBlock* RowText = Cast<UTextBlock>(RowWidget->GetWidgetFromName(TEXT("Row_Text")));
                if (RowText)
                    RowText->SetText(FText::FromString(Line));
                Attributes_Box->AddChild(RowWidget);
                continue;
            }
        }

        // Fallback: plain TextBlock
        UTextBlock* TB = NewObject<UTextBlock>(this);
        TB->SetText(FText::FromString(Line));
        Attributes_Box->AddChild(TB);
    }
}

void UPlayerStatsWidget::RefreshEffects(const FPlayerStatsUpdateStruct& Stats)
{
    if (!Effects_Box)
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerStatsWidget::RefreshEffects - Effects_Box is NULL (add UVerticalBox named 'Effects_Box' to WBP_PlayerStats)"));
        return;
    }
    Effects_Box->ClearChildren();

    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();

    // Group entries by slug so we show one effect row per unique effect name.
    TArray<FString> OrderedSlugs;
    TMap<FString, TArray<FActiveEffectEntry>> Grouped;
    for (const FActiveEffectEntry& Effect : Stats.activeEffects)
    {
        if (!Grouped.Contains(Effect.slug))
            OrderedSlugs.Add(Effect.slug);
        Grouped.FindOrAdd(Effect.slug).Add(Effect);
    }

    for (const FString& Slug : OrderedSlugs)
    {
        const TArray<FActiveEffectEntry>& Entries = Grouped[Slug];
        if (Entries.Num() == 0) continue;

        // Representative entry carries slug / type / expiresAt
        const FActiveEffectEntry& Rep = Entries[0];

        // Skip entirely expired effects
        if (Rep.expiresAt > 0 && Rep.expiresAt <= NowSec) continue;

        // Header line: "Resurrection Sickness  45s"
        FString Header;
        FString DisplayName = Slug;
        FString Category;
        {
            if (UGameInstance* GI = GetGameInstance())
            {
                if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                {
                    const FText LocName = Loc->GetEffectDisplayName(Slug);
                    if (!LocName.IsEmpty()) DisplayName = LocName.ToString();
                }
            }
        }
        if (Rep.expiresAt == 0)
            Header = FString::Printf(TEXT("[%s] %s  permanent"), *Rep.effectTypeSlug, *DisplayName);
        else
        {
            const int64 SecsLeft = Rep.expiresAt - NowSec;
            Header = FString::Printf(TEXT("[%s] %s  %ds"), *Rep.effectTypeSlug, *DisplayName, (int32)SecsLeft);
        }

        auto AddRow = [&](const FString& Line)
        {
            if (EffectRowClass)
            {
                UUserWidget* RowWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), EffectRowClass);
                if (RowWidget)
                {
                    if (UTextBlock* RowText = Cast<UTextBlock>(RowWidget->GetWidgetFromName(TEXT("Row_Text"))))
                        RowText->SetText(FText::FromString(Line));
                    Effects_Box->AddChild(RowWidget);
                    return;
                }
            }
            UTextBlock* TB = NewObject<UTextBlock>(this);
            TB->SetText(FText::FromString(Line));
            Effects_Box->AddChild(TB);
        };

        AddRow(Header);

        // Attribute modifier lines indented under the header
        for (const FActiveEffectEntry& Mod : Entries)
        {
            if (Mod.attributeSlug.IsEmpty()) continue;
            const FString Sign = Mod.value >= 0.0f ? TEXT("+") : TEXT("");
            AddRow(FString::Printf(TEXT("    %s: %s%.0f"), *Mod.attributeSlug, *Sign, Mod.value));
        }
    }
}

// ---------------------------------------------------------------------------
// Effect countdown tick
// ---------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

