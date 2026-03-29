#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/UserWidget.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UPlayerStatsWidget::HandleCloseButtonClicked);

    // Center on screen
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D((W - Size.X) * 0.5f, (H - Size.Y) * 0.5f);
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerStatsWidget::NativeDestruct()
{
    if (StatsManager)
        StatsManager->OnStatsUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleStatsUpdated);

    Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::BindToStatsManager(UPlayerStatsManager* InStatsManager)
{
    if (StatsManager)
        StatsManager->OnStatsUpdated.RemoveDynamic(this, &UPlayerStatsWidget::HandleStatsUpdated);

    StatsManager = InStatsManager;

    if (StatsManager)
    {
        StatsManager->OnStatsUpdated.AddDynamic(this, &UPlayerStatsWidget::HandleStatsUpdated);
        // Populate immediately if we already have data
        const FPlayerStatsUpdateStruct& Cached = StatsManager->GetCachedStats();
        if (Cached.characterId > 0)
            RefreshAll(Cached);
    }
}

void UPlayerStatsWidget::OpenStats()
{
    const FPlayerStatsUpdateStruct& Stats =
        (CachedStats.characterId > 0) ? CachedStats
        : (StatsManager ? StatsManager->GetCachedStats() : CachedStats);
    RefreshAll(Stats);

    SetVisibility(ESlateVisibility::Visible);
    UpdateCountdownTimer();
    OnStatsVisibilityChanged.Broadcast();
}

void UPlayerStatsWidget::CloseStats()
{
    SetVisibility(ESlateVisibility::Collapsed);

    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(EffectCountdownTimer);

    OnStatsVisibilityChanged.Broadcast();
}

void UPlayerStatsWidget::ToggleStats()
{
    if (GetVisibility() == ESlateVisibility::Visible)
        CloseStats();
    else
        OpenStats();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void UPlayerStatsWidget::HandleCloseButtonClicked()
{
    CloseStats();
}

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
    RefreshVitals(Stats);
    RefreshAttributes(Stats);
    RefreshEffects(Stats);
}

void UPlayerStatsWidget::RefreshVitals(const FPlayerStatsUpdateStruct& Stats)
{
    // Level
    if (Level_Text)
        Level_Text->SetText(FText::FromString(FString::Printf(TEXT("Level %d"), Stats.level)));

    // HP
    if (HP_Bar)
    {
        const float Pct = Stats.healthMax > 0 ? (float)Stats.healthCurrent / (float)Stats.healthMax : 0.f;
        HP_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (HP_Text)
        HP_Text->SetText(FText::FromString(FString::Printf(TEXT("HP: %d / %d"), Stats.healthCurrent, Stats.healthMax)));

    // MP
    if (MP_Bar)
    {
        const float Pct = Stats.manaMax > 0 ? (float)Stats.manaCurrent / (float)Stats.manaMax : 0.f;
        MP_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (MP_Text)
        MP_Text->SetText(FText::FromString(FString::Printf(TEXT("MP: %d / %d"), Stats.manaCurrent, Stats.manaMax)));

    // XP bar: (current - levelStart) / (nextLevel - levelStart)
    if (XP_Bar)
    {
        const int32 Range = Stats.experienceNextLevel - Stats.experienceLevelStart;
        const int32 Delta = Stats.experienceCurrent  - Stats.experienceLevelStart;
        const float Pct   = Range > 0 ? (float)Delta / (float)Range : 0.f;
        XP_Bar->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (XP_Text)
        XP_Text->SetText(FText::FromString(FString::Printf(TEXT("XP: %d / %d"), Stats.experienceCurrent, Stats.experienceNextLevel)));

    // Debt: show in red when non-zero, hide otherwise
    if (Debt_Text)
    {
        if (Stats.experienceDebt > 0)
        {
            Debt_Text->SetText(FText::FromString(FString::Printf(TEXT("Debt: %d XP"), Stats.experienceDebt)));
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
        Weight_Text->SetText(FText::FromString(FString::Printf(TEXT("Weight: %.1f / %.1f kg"), Stats.weightCurrent, Stats.weightMax)));
}

void UPlayerStatsWidget::RefreshAttributes(const FPlayerStatsUpdateStruct& Stats)
{
    if (!Attributes_Box) return;
    Attributes_Box->ClearChildren();

    for (const FStatAttributeEntry& Attr : Stats.attributes)
    {
        const int32 EffInt = static_cast<int32>(Attr.effective);
        const int32 Bonus = EffInt - static_cast<int32>(Attr.base);

        FString Line;
        if (Bonus > 0)
            Line = FString::Printf(TEXT("%s:  %d  (+%d)"), *Attr.slug, EffInt, Bonus);
        else if (Bonus < 0)
            Line = FString::Printf(TEXT("%s:  %d  (%d)"), *Attr.slug, EffInt, Bonus);
        else
            Line = FString::Printf(TEXT("%s:  %d"), *Attr.slug, EffInt);

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
    if (!Effects_Box) return;
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

        // Header line: "[debuff] Resurrection Sickness  45s"
        FString Header;
        if (Rep.expiresAt == 0)
            Header = FString::Printf(TEXT("[%s] %s  permanent"), *Rep.effectTypeSlug, *Slug);
        else
        {
            const int64 SecsLeft = Rep.expiresAt - NowSec;
            Header = FString::Printf(TEXT("[%s] %s  %ds"), *Rep.effectTypeSlug, *Slug, (int32)SecsLeft);
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

void UPlayerStatsWidget::TickEffectCountdowns()
{
    if (GetVisibility() != ESlateVisibility::Visible)
        return;

    const FPlayerStatsUpdateStruct& Stats =
        (CachedStats.characterId > 0) ? CachedStats
        : (StatsManager ? StatsManager->GetCachedStats() : CachedStats);

    RefreshEffects(Stats);

    // If all timed effects have expired, stop ticking
    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();
    const bool bAnyTimedActive = Stats.activeEffects.ContainsByPredicate(
        [NowSec](const FActiveEffectEntry& E)
        {
            return E.expiresAt > NowSec;
        });

    if (!bAnyTimedActive)
    {
        if (UWorld* World = GetWorld())
            World->GetTimerManager().ClearTimer(EffectCountdownTimer);
    }
}

void UPlayerStatsWidget::UpdateCountdownTimer()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FPlayerStatsUpdateStruct& Stats =
        (CachedStats.characterId > 0) ? CachedStats
        : (StatsManager ? StatsManager->GetCachedStats() : CachedStats);

    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();
    const bool bAnyTimedActive = Stats.activeEffects.ContainsByPredicate(
        [NowSec](const FActiveEffectEntry& E)
        {
            return E.expiresAt > NowSec;
        });

    if (bAnyTimedActive && GetVisibility() == ESlateVisibility::Visible)
    {
        if (!EffectCountdownTimer.IsValid())
            World->GetTimerManager().SetTimer(
                EffectCountdownTimer,
                this,
                &UPlayerStatsWidget::TickEffectCountdowns,
                1.0f,
                /*bLoop=*/true);
    }
    else
    {
        World->GetTimerManager().ClearTimer(EffectCountdownTimer);
    }
}

// ---------------------------------------------------------------------------
// Drag support (same pattern as VendorShopWidget)
// ---------------------------------------------------------------------------

FReply UPlayerStatsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldDrag = !DragHandle;
        if (DragHandle)
        {
            const FGeometry G = DragHandle->GetCachedGeometry();
            const FVector2D L = G.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D S = G.GetLocalSize();
            bShouldDrag = (L.X >= 0 && L.X <= S.X && L.Y >= 0 && L.Y <= S.Y);
        }
        if (bShouldDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            DragOffset = InMouseEvent.GetScreenSpacePosition() / Scale - CurrentViewportPosition;
            bDragging  = true;
            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

FReply UPlayerStatsWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            return FReply::Handled().ReleaseMouseCapture();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply UPlayerStatsWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging) { UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition()); return FReply::Handled(); }
    return FReply::Unhandled();
}

void UPlayerStatsWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VP  = FVector2D(W, H) / Scale;
    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 600);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, VP.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, VP.Y - Size.Y);
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
