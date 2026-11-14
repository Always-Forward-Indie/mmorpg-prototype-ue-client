#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MyGameInstance.h"
#include "Services/TimeSyncService.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Utils/NetCore/MobTargetFrameRule.h"

void UMobTargetFrameWidget::ShowCast(const FString& SkillName, const FString& SkillSlug, float CastTimeSec, int64 CastStartedAtMs, bool bInterruptible)
{
    // Instant casts and the basic attack never show a bar (row stays collapsed).
    if (!FMobTargetCastRule::ShouldShowCastbar(CastTimeSec, SkillSlug))
    {
        HideCast();
        return;
    }

    CachedCastSkillName = SkillName;
    CachedCastSkillSlug = SkillSlug;
    CachedCastTotalSec = CastTimeSec;
    CachedCastStartedAtMs = CastStartedAtMs;
    if (UWorld* W = GetWorld())
    {
        FallbackCastStartWorldSec = static_cast<double>(W->GetTimeSeconds());
    }
    bIsCasting = true;

    if (CastNameText)
    {
        CastNameText->SetText(FText::FromString(SkillName));
    }
    LoadCastIconFromRepo(SkillSlug);
    // No interruptible casts exist server-side (all casts complete once
    // started); the icon stays hidden until the server ships the per-skill
    // flag (tracker mirenhold #134). Accepted for API parity with the plan.
    if (InterruptIcon)
    {
        InterruptIcon->SetVisibility(ESlateVisibility::Collapsed);
    }
    TickCast();
    SetCastRowVisibility(ESlateVisibility::HitTestInvisible);
}

void UMobTargetFrameWidget::TickCast()
{
    if (!bIsCasting || CachedCastTotalSec <= 0.0f) return;

    float Progress = 1.0f;
    float Remaining = 0.0f;
    const int64 ServerNowMs = ResolveServerNowMs();
    if (ServerNowMs >= 0 && CachedCastStartedAtMs > 0)
    {
        Progress = FMobTargetCastRule::ProgressAt(ServerNowMs, CachedCastStartedAtMs, CachedCastTotalSec);
        Remaining = FMobTargetCastRule::RemainingSecAt(ServerNowMs, CachedCastStartedAtMs, CachedCastTotalSec);
    }
    else if (UWorld* W = GetWorld())
    {
        // Degraded clock only: time-sync unavailable (offline/early init).
        const double Elapsed = static_cast<double>(W->GetTimeSeconds()) - FallbackCastStartWorldSec;
        Progress = static_cast<float>(FMath::Clamp(Elapsed / static_cast<double>(CachedCastTotalSec), 0.0, 1.0));
        Remaining = static_cast<float>(FMath::Max(0.0, static_cast<double>(CachedCastTotalSec) - Elapsed));
    }

    if (CastBar)
    {
        CastBar->SetPercent(Progress);
    }
    if (CastTimeText)
    {
        CastTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Remaining)));
    }

    if (Progress >= 1.0f)
    {
        HideCast();
    }
}

void UMobTargetFrameWidget::HideCast()
{
    bIsCasting = false;
    CachedCastTotalSec = 0.0f;
    SetCastRowVisibility(ESlateVisibility::Collapsed);
}

int64 UMobTargetFrameWidget::ResolveServerNowMs() const
{
    if (const UWorld* W = GetWorld())
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance()))
        {
            if (const UTimeSyncService* TS = GI->GetTimeSyncService())
            {
                return TS->GetEstimatedServerTimeMs(EServerType::ChunkServer);
            }
        }
    }
    return -1;
}

void UMobTargetFrameWidget::SetCastRowVisibility(ESlateVisibility Vis)
{
    // The frame itself stays visible (health/name remain); only the row collapses.
    if (CastBar)      { CastBar->SetVisibility(Vis); }
    if (CastNameText) { CastNameText->SetVisibility(Vis); }
    if (CastTimeText) { CastTimeText->SetVisibility(Vis); }
    if (CastSkillIcon)
    {
        // No icon when nothing was resolved; never show an empty frame.
        const bool bShowIcon = Vis != ESlateVisibility::Collapsed && CastSkillIcon->GetBrush().GetResourceObject() != nullptr;
        CastSkillIcon->SetVisibility(bShowIcon ? Vis : ESlateVisibility::Collapsed);
    }
    if (InterruptIcon)
    {
        InterruptIcon->SetVisibility(ESlateVisibility::Collapsed);
    }
}

/** Resolve the cast skill icon from the shared skill repository (same DT as the player skill list). */
void UMobTargetFrameWidget::LoadCastIconFromRepo(const FString& SkillSlug)
{
    if (CastSkillIcon)
    {
        CastSkillIcon->SetBrushFromTexture(nullptr);
        CastSkillIcon->SetVisibility(ESlateVisibility::Collapsed);
    }
    UWorld* W = GetWorld();
    if (!W || SkillSlug.IsEmpty()) return;

    UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance());
    if (!GI) return;
    USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository();
    if (!Repo) return;

    // NB: GetDefinition returns by value — copy the soft pointer, never bind it.
    const TSoftObjectPtr<UTexture2D> IconPtr = Repo->GetDefinition(SkillSlug).skillIcon;
    if (IconPtr.IsNull()) return;

    if (UTexture2D* Already = IconPtr.Get())
    {
        if (CastSkillIcon)
        {
            CastSkillIcon->SetBrushFromTexture(Already);
            CastSkillIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        return;
    }

    TWeakObjectPtr<UMobTargetFrameWidget> WeakThis(this);
    TSoftObjectPtr<UTexture2D> IconSoft = IconPtr;
    FString WantedSlug = SkillSlug;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        IconSoft.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, IconSoft, WantedSlug]()
        {
            if (!WeakThis.IsValid()) return;
            if (!WeakThis->bIsCasting || WeakThis->CachedCastSkillSlug != WantedSlug) return;
            if (UTexture2D* Loaded = IconSoft.Get())
            {
                if (WeakThis->CastSkillIcon)
                {
                    WeakThis->CastSkillIcon->SetBrushFromTexture(Loaded);
                    WeakThis->CastSkillIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
                }
            }
        })
    );
}

