#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/EffectSlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Services/LocalizationSubsystem.h"

void UActiveEffectsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Tick every second to update countdown labels without rebuilding all slots.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TickTimerHandle, this,
            &UActiveEffectsWidget::OnSecondTick, 1.0f, true);
    }
}

void UActiveEffectsWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
    }
    Super::NativeDestruct();
}

void UActiveEffectsWidget::RefreshEffects(const TArray<FActiveEffectEntry>& Effects)
{
    UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] UActiveEffectsWidget::RefreshEffects called with %d effects, Effects_Container=%s"),
        Effects.Num(), Effects_Container ? TEXT("valid") : TEXT("NULL"));
    CachedEffects = Effects;
    RebuildSlots();
}

void UActiveEffectsWidget::ClearEffects()
{
    CachedEffects.Empty();
    if (Effects_Container)
    {
        Effects_Container->ClearChildren();
    }
}

void UActiveEffectsWidget::OnSecondTick()
{
    // Only update timer labels � no full rebuild needed.
    if (!Effects_Container) return;

    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();

    // Check if any unique effect slug has expired; remove all entries for it.
    // Effects with expiresAt == 0 are permanent / passive � never expire client-side.
    TSet<FString> ExpiredSlugs;
    for (const FActiveEffectEntry& Effect : CachedEffects)
    {
        if (Effect.expiresAt > 0 && Effect.expiresAt <= NowSec)
            ExpiredSlugs.Add(Effect.slug);
    }

    if (ExpiredSlugs.Num() > 0)
    {
        CachedEffects.RemoveAll([&](const FActiveEffectEntry& E)
        {
            return ExpiredSlugs.Contains(E.slug);
        });
        RebuildSlots();
        return;
    }

    // Refresh timers on existing slot widgets
    for (int32 i = 0; i < Effects_Container->GetChildrenCount(); ++i)
    {
        if (UEffectSlotWidget* SlotWidget = Cast<UEffectSlotWidget>(Effects_Container->GetChildAt(i)))
        {
            SlotWidget->RefreshTimer();
        }
    }
}

void UActiveEffectsWidget::GroupEffects(const TArray<FActiveEffectEntry>& InEffects,
                                         TArray<FActiveEffectEntry>& OutRepresentatives,
                                         TMap<FString, TArray<FActiveEffectEntry>>& OutGrouped)
{
    OutRepresentatives.Empty();
    OutGrouped.Empty();

    for (const FActiveEffectEntry& Entry : InEffects)
    {
        if (!OutGrouped.Contains(Entry.slug))
        {
            OutRepresentatives.Add(Entry);
            OutGrouped.Add(Entry.slug, TArray<FActiveEffectEntry>());
        }
        OutGrouped[Entry.slug].Add(Entry);
    }
}

void UActiveEffectsWidget::RebuildSlots()
{
    UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] UActiveEffectsWidget::RebuildSlots: Effects_Container=%s CachedEffects=%d EffectSlotClass=%s"),
        Effects_Container ? TEXT("valid") : TEXT("NULL"),
        CachedEffects.Num(),
        EffectSlotClass ? TEXT("valid") : TEXT("NULL"));

    if (!Effects_Container) return;
    Effects_Container->ClearChildren();

    const int64 NowSec = FDateTime::UtcNow().ToUnixTimestamp();

    TArray<FActiveEffectEntry> Representatives;
    TMap<FString, TArray<FActiveEffectEntry>> Grouped;
    GroupEffects(CachedEffects, Representatives, Grouped);

    for (const FActiveEffectEntry& Rep : Representatives)
    {
        // Skip already-expired timed effects; permanent/passive effects (expiresAt == 0) are always shown.
        if (Rep.expiresAt > 0 && Rep.expiresAt <= NowSec) continue;

        const TArray<FActiveEffectEntry>& Modifiers = Grouped[Rep.slug];

        if (EffectSlotClass)
        {
            UEffectSlotWidget* NewSlot = CreateWidget<UEffectSlotWidget>(GetOwningPlayer(), EffectSlotClass);
            if (NewSlot)
            {
                NewSlot->SetupSlotGrouped(Rep, Modifiers, EffectDefinitionTable.Get());
                UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Effects_Container->AddChild(NewSlot));
                if (BoxSlot)
                {
                    BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
                }
                continue;
            }
        }

        // Fallback: plain text label when no slot class is assigned.
        UTextBlock* TB = NewObject<UTextBlock>(this);
        const FString TimerStr = Rep.expiresAt > 0
            ? FString::Printf(TEXT("%ds"), (int32)FMath::Max<int64>(Rep.expiresAt - NowSec, 0))
            : TEXT("?");
        FString DisplayName = Rep.slug;
        if (UGameInstance* GI = GetGameInstance())
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                DisplayName = Loc->GetEffectDisplayName(Rep.slug).ToString();
            }
        }
        TB->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s"), *DisplayName, *TimerStr)));
        Effects_Container->AddChild(TB);
    }
}
