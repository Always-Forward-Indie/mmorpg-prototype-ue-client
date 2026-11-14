#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Gameplay/UI/EffectSlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/NetCore/MobTargetFrameRule.h"
#include "Utils/NetCore/CooldownFormat.h"

void UMobTargetFrameWidget::UpdateStatusEffects(const TArray<FAppliedEffectData>& Effects)
{
    // Group by slug (one icon per effect, longest duration wins) — same rule
    // as the player effects bar, so the two rows speak one visual language.
    TMap<FString, FCachedTargetEffect> Grouped;
    const int64 ServerNowMs = ResolveServerNowMs();

    for (const FAppliedEffectData& Fx : Effects)
    {
        if (Fx.effectName.IsEmpty()) continue;
        const double Dur = static_cast<double>(Fx.duration);
        FCachedTargetEffect* Found = Grouped.Find(Fx.effectName);
        if (!Found)
        {
            FCachedTargetEffect E;
            E.Slug = Fx.effectName;
            E.EffectType = Fx.effectType;
            E.Value = Fx.value;
            E.DurationSec = Dur;
            E.ExpiresAtServerMs = (Dur <= 0.0 || ServerNowMs < 0)
                ? 0
                : ServerNowMs + static_cast<int64>(Dur * 1000.0);
            Grouped.Add(Fx.effectName, MoveTemp(E));
        }
        else if (Dur > Found->DurationSec)
        {
            Found->DurationSec = Dur;
            Found->Value = Fx.value;
            Found->EffectType = Fx.effectType;
            if (Dur > 0.0 && ServerNowMs >= 0)
            {
                Found->ExpiresAtServerMs = ServerNowMs + static_cast<int64>(Dur * 1000.0);
            }
        }
    }

    CachedTargetEffects.Reset();
    for (auto& Pair : Grouped)
    {
        CachedTargetEffects.Add(MoveTemp(Pair.Value));
    }
    EffectsTickAccum = 0.0;
    EffectsArrivedWorldSec = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
    RebuildEffectSlots();
}

double UMobTargetFrameWidget::RemainingSecFor(const FCachedTargetEffect& E) const
{
    if (E.DurationSec <= 0.0) return 1e9; // permanent: never expires
    const int64 ServerNowMs = ResolveServerNowMs();
    if (ServerNowMs >= 0 && E.ExpiresAtServerMs > 0)
    {
        return static_cast<double>(E.ExpiresAtServerMs - ServerNowMs) / 1000.0;
    }
    if (GetWorld())
    {
        const double Elapsed = static_cast<double>(GetWorld()->GetTimeSeconds()) - EffectsArrivedWorldSec;
        return E.DurationSec - Elapsed;
    }
    return E.DurationSec;
}

void UMobTargetFrameWidget::RebuildEffectSlots()
{
    if (!StatusEffectsBox) return;
    StatusEffectsBox->ClearChildren();

    // A fresh result batch supersedes the row: drop already-expired entries.
    for (int32 i = CachedTargetEffects.Num() - 1; i >= 0; --i)
    {
        if (RemainingSecFor(CachedTargetEffects[i]) <= 0.0)
        {
            CachedTargetEffects.RemoveAt(i);
        }
    }

    if (CachedTargetEffects.IsEmpty())
    {
        StatusEffectsBox->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // Priority first, then shortest remaining — production rule, pinned by spec.
    CachedTargetEffects.Sort([](const FCachedTargetEffect& A, const FCachedTargetEffect& B)
    {
        return FMobTargetEffectSort::EffectLess(A.Slug, static_cast<float>(A.DurationSec),
            B.Slug, static_cast<float>(B.DurationSec));
    });

    const int64 NowUnixSec = FDateTime::UtcNow().ToUnixTimestamp();
    const int32 Visible = FMath::Min(CachedTargetEffects.Num(), FMobTargetEffectSort::MaxVisibleEffects);

    for (int32 i = 0; i < Visible; ++i)
    {
        const FCachedTargetEffect& E = CachedTargetEffects[i];
        const double Remaining = FMath::Max(0.0, RemainingSecFor(E));
        const int64 SlotExpiresAt = (E.DurationSec <= 0.0)
            ? 0
            : NowUnixSec + static_cast<int64>(Remaining);

        UEffectSlotWidget* EffectSlot = nullptr;
        if (EffectSlotClass && GetOwningPlayer())
        {
            EffectSlot = CreateWidget<UEffectSlotWidget>(GetOwningPlayer(), EffectSlotClass);
        }
        if (EffectSlot)
        {
            FActiveEffectEntry Entry;
            Entry.slug = E.Slug;
            Entry.effectType = E.EffectType;
            Entry.effectTypeSlug = E.EffectType;
            Entry.value = static_cast<float>(E.Value);
            Entry.expiresAt = SlotExpiresAt;
            EffectSlot->SetupSlot(Entry, EffectDefinitionTable.Get());
            if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(StatusEffectsBox->AddChild(EffectSlot)))
            {
                BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
            }
            continue;
        }

        // Fallback: plain slug + countdown when no slot class is assigned.
        UTextBlock* TB = NewObject<UTextBlock>(this);
        FString DisplayName = E.Slug;
        if (UGameInstance* GI = GetGameInstance())
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                DisplayName = Loc->GetEffectDisplayName(E.Slug).ToString();
            }
        }
        const FString TimerStr = (E.DurationSec <= 0.0)
            ? FString(TEXT("\u221E"))
            : FCooldownFormat::Format(static_cast<float>(Remaining));
        TB->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s"), *DisplayName, *TimerStr)));
        StatusEffectsBox->AddChild(TB);
    }

    if (CachedTargetEffects.Num() > Visible)
    {
        UTextBlock* Overflow = NewObject<UTextBlock>(this);
        Overflow->SetText(FText::FromString(
            FString::Printf(TEXT("+%d"), CachedTargetEffects.Num() - Visible)));
        StatusEffectsBox->AddChild(Overflow);
    }

    StatusEffectsBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMobTargetFrameWidget::RefreshEffectTimers()
{
    // A corpse freezes its row (no ticking on the dead).
    if (bCachedDead || CachedTargetEffects.IsEmpty()) return;

    bool bChanged = false;
    for (int32 i = CachedTargetEffects.Num() - 1; i >= 0; --i)
    {
        if (RemainingSecFor(CachedTargetEffects[i]) <= 0.0)
        {
            CachedTargetEffects.RemoveAt(i);
            bChanged = true;
        }
    }

    if (!StatusEffectsBox) return;
    if (CachedTargetEffects.IsEmpty())
    {
        StatusEffectsBox->ClearChildren();
        StatusEffectsBox->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    if (bChanged)
    {
        RebuildEffectSlots();
        return;
    }
    for (int32 i = 0; i < StatusEffectsBox->GetChildrenCount(); ++i)
    {
        if (UEffectSlotWidget* ChildSlot = Cast<UEffectSlotWidget>(StatusEffectsBox->GetChildAt(i)))
        {
            ChildSlot->RefreshTimer();
        }
    }
}

void UMobTargetFrameWidget::ClearTargetEffects()
{
    CachedTargetEffects.Reset();
    EffectsTickAccum = 0.0;
    if (StatusEffectsBox)
    {
        StatusEffectsBox->ClearChildren();
        StatusEffectsBox->SetVisibility(ESlateVisibility::Collapsed);
    }
}
