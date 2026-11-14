#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Services/TimeSyncService.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/DataStructs.h"
#include "Utils/NetCore/MobTargetFrameRule.h"

// S5b: target-frame level chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "MobTargetFrame"

void UMobTargetFrameWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.AddDynamic(this, &UMobTargetFrameWidget::HandleLocaleChanged);
        }
    }
    ApplyDeadChrome();
}

void UMobTargetFrameWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.RemoveDynamic(this, &UMobTargetFrameWidget::HandleLocaleChanged);
        }
    }
    Super::NativeDestruct();
}

void UMobTargetFrameWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    TickCast();
}

void UMobTargetFrameWidget::SetMobInfo(const FString& MobSlug,
                                        const FString& MobName,
                                        int32 MobLevel,
                                        int32 CurrentHP, int32 MaxHP,
                                        bool bIsAggro,
                                        UTexture2D* Icon)
{
    // Cache for locale-change refresh
    CachedMobSlug  = MobSlug;
    CachedMobName  = MobName;
    CachedMobLevel = MobLevel;
    CachedCurrentHP = CurrentHP;
    CachedMaxHP    = MaxHP;
    bCachedAggro   = bIsAggro;
    bHasTarget     = true;
    // A new lock is a fresh target: rank/dead state is re-supplied by the
    // SetRank/SetDead calls that follow (or stays hidden for players).
    CachedRankCode.Empty();
    CachedChampionKind.Empty();
    bCachedChampion = false;
    CachedViewerLevel = 0;
    bCachedDead = false;
    // A new lock cancels any in-flight cast display from the previous target.
    HideCast();

    // --- Portrait icon ---
    // Priority: 1) caller-supplied Icon (CachedIcon from BasicMOB, already in memory)
    //           2) DataTable lookup by MobSlug (async-safe, same pattern as BestiaryMobRowWidget)
    //           3) DefaultIcon fallback (for mob targets only)
    //           4) Hidden for player/NPC/self targets (no MobSlug and no Icon)
    if (PortraitImage)
    {
        if (Icon)
        {
            // Fast path: caller already resolved the texture (BasicMOB::CachedIcon)
            ApplyPortraitTexture(Icon);
        }
        else if (!MobSlug.IsEmpty())
        {
            // Fetch from MobDefinitionTable - covers the case where CachedIcon is null
            // (e.g. mob was locked before SetupMobVisual finished, or Icon not set in DT)
            LoadPortraitFromTable(MobSlug);
        }
        else
        {
            // Player, NPC, or self target - no portrait available, hide the image
            CachedPortrait = nullptr;
            PortraitImage->SetBrushFromTexture(nullptr);
            PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    RefreshLocalizedTexts();

    // Aggro icon
    if (AggroIcon)
    {
        AggroIcon->SetVisibility(bIsAggro ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    ApplyRankChrome();
    ApplyDeadChrome();
    UpdateHP(CurrentHP, MaxHP);

    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMobTargetFrameWidget::SetRank(const FString& RankCode, bool bIsChampion, const FString& ChampionKind, int32 ViewerLevel)
{
    CachedRankCode = FMobTargetRankRule::NormalizeRankCode(RankCode, bIsChampion);
    bCachedChampion = bIsChampion;
    CachedChampionKind = ChampionKind;
    CachedViewerLevel = ViewerLevel;
    ApplyRankChrome();
}

void UMobTargetFrameWidget::SetDead(bool bIsDead)
{
    if (bCachedDead == bIsDead && bHasTarget)
    {
        return;
    }
    bCachedDead = bIsDead;
    ApplyDeadChrome();
    if (bIsDead)
    {
        // A corpse never keeps casting: stop the bar, freeze effect ticks.
        HideCast();
        UpdateHP(0, CachedMaxHP);
    }
}

void UMobTargetFrameWidget::UpdateHP(int32 CurrentHP, int32 MaxHP)
{
    CachedCurrentHP = CurrentHP;
    CachedMaxHP = MaxHP;
    const int32 ShownHP = bCachedDead ? 0 : FMath::Max(CurrentHP, 0);

    if (MobHealthBar && MaxHP > 0)
    {
        MobHealthBar->SetPercent(static_cast<float>(ShownHP) / static_cast<float>(MaxHP));
        if (bCachedDead)
        {
            MobHealthBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.38f, 1.0f));
        }
        else
        {
            MobHealthBar->SetFillColorAndOpacity(FLinearColor::White);
        }
    }

    if (MobHealthText)
    {
        MobHealthText->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d"), ShownHP, MaxHP)));
    }
}

void UMobTargetFrameWidget::ClearTarget()
{
    bHasTarget = false;
    bCachedDead = false;
    CachedRankCode.Empty();
    CachedPortrait = nullptr;
    HideCast();
    SetVisibility(ESlateVisibility::Collapsed);

    if (MobNameText)   MobNameText->SetText(FText::GetEmpty());
    if (MobLevelText)  MobLevelText->SetText(FText::GetEmpty());
    if (RankLabelText) RankLabelText->SetVisibility(ESlateVisibility::Collapsed);
    if (DeadOverlay)   DeadOverlay->SetVisibility(ESlateVisibility::Collapsed);
    if (DeadText)      DeadText->SetVisibility(ESlateVisibility::Collapsed);
    if (MobHealthBar)
    {
        MobHealthBar->SetPercent(0.f);
        MobHealthBar->SetFillColorAndOpacity(FLinearColor::White);
    }
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
    if (InterruptIcon)
    {
        InterruptIcon->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UMobTargetFrameWidget::ApplyPortraitTexture(UTexture2D* Texture)
{
    if (!PortraitImage || !Texture) return;
    CachedPortrait = Texture;
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
    FString WantedSlug = MobSlug;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        IconSoft.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, IconSoft, WantedSlug]()
        {
            if (!WeakThis.IsValid()) return;
            // Stale load guard: the lock may have moved on while streaming.
            if (WeakThis->CachedMobSlug != WantedSlug) return;
            if (UTexture2D* Loaded = IconSoft.Get())
            {
                WeakThis->ApplyPortraitTexture(Loaded);
            }
        })
    );
}

void UMobTargetFrameWidget::RefreshLocalizedTexts()
{
    // Localized name — fall back to raw MobName if slug lookup fails
    if (MobNameText)
    {
        FText DisplayName;
        if (!CachedMobSlug.IsEmpty())
        {
            if (UWorld* W = GetWorld())
            {
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance()))
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        DisplayName = Loc->GetMobDisplayName(CachedMobSlug);
                    }
                }
            }
        }
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(CachedMobName);
        }

        MobNameText->SetText(DisplayName);

        // Red for aggressive mobs, yellow for passive
        const FLinearColor NameColor = bCachedAggro
            ? FLinearColor(1.f, 0.2f, 0.2f, 1.f)
            : FLinearColor(1.f, 0.85f, 0.f, 1.f);
        MobNameText->SetColorAndOpacity(bCachedDead ? FSlateColor(FLinearColor(0.6f, 0.6f, 0.62f, 1.f)) : FSlateColor(NameColor));
    }

    ApplyRankChrome();
    ApplyDeadChrome();
}

void UMobTargetFrameWidget::ApplyRankChrome()
{
    const bool bShow = FMobTargetRankRule::ShouldShowRankLabel(CachedRankCode);

    if (RankLabelText)
    {
        if (!bShow)
        {
            RankLabelText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            FText RankText;
            if (CachedRankCode.Equals(TEXT("boss"), ESearchCase::IgnoreCase))
            {
                RankText = LOCTEXT("RankBoss", "Босс");
            }
            else if (CachedRankCode.Equals(TEXT("elite"), ESearchCase::IgnoreCase))
            {
                RankText = LOCTEXT("RankElite", "Элита");
            }
            else if (CachedRankCode.Equals(TEXT("champion"), ESearchCase::IgnoreCase))
            {
                if (!CachedChampionKind.IsEmpty())
                {
                    RankText = FText::Format(LOCTEXT("RankChampionKind", "Чемпион • {0}"),
                        FText::FromString(CachedChampionKind));
                }
                else
                {
                    RankText = LOCTEXT("RankChampion", "Чемпион");
                }
            }
            else
            {
                RankText = LOCTEXT("RankNormal", "Обычный");
            }
            RankLabelText->SetText(RankText);
            RankLabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }

    const FLinearColor BorderColor = FMobTargetRankRule::BorderColorForRank(CachedRankCode);
    if (RankBorder)
    {
        RankBorder->SetBrushColor(BorderColor);
    }
    else if (RankLabelText && bShow)
    {
        // Graceful degradation for BPs predating the frame border: tint the label.
        RankLabelText->SetColorAndOpacity(FSlateColor(BorderColor));
    }

    // Level colour follows the viewer diff; lethal targets also carry a marker.
    if (MobLevelText && bHasTarget)
    {
        const FMobTargetLevelRule::FLevelStyle Style =
            FMobTargetLevelRule::StyleForDiff(CachedViewerLevel, CachedMobLevel);
        FText LevelText = FText::Format(LOCTEXT("LevelValue", "LVL: {0}"), FText::AsNumber(CachedMobLevel));
        if (Style.bLethalMarker)
        {
            LevelText = FText::FromString(LevelText.ToString() + FMobTargetLevelRule::LethalMarker());
        }
        MobLevelText->SetText(LevelText);
        MobLevelText->SetColorAndOpacity(FSlateColor(Style.Color));
    }
}

void UMobTargetFrameWidget::ApplyDeadChrome()
{
    if (DeadOverlay)
    {
        DeadOverlay->SetVisibility(bCachedDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (DeadText)
    {
        if (bCachedDead)
        {
            DeadText->SetText(LOCTEXT("DeadLabel", "МЕРТВ"));
            DeadText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            DeadText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    if (MobHealthBar && bCachedDead)
    {
        MobHealthBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.38f, 1.0f));
    }
}

void UMobTargetFrameWidget::HandleLocaleChanged(const FString& NewLocale)
{
    if (!bHasTarget) return;
    // Re-apply the cached portrait (never wipe to nullptr) and refresh texts.
    if (CachedPortrait && PortraitImage)
    {
        PortraitImage->SetBrushFromTexture(CachedPortrait);
        PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    RefreshLocalizedTexts();
    if (MobLevelText && bHasTarget)
    {
        ApplyRankChrome();
    }
    // In-flight cast name follows the locale like everything else.
    if (bIsCasting && CastNameText && !CachedCastSkillSlug.IsEmpty())
    {
        if (UWorld* W = GetWorld())
        {
            if (UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance()))
            {
                if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                {
                    const FString Resolved = UCombatSystemManager::ResolveSkillDisplayNameForUI(
                        CachedCastSkillSlug, TEXT(""), Loc, GI->GetSkillDefinitionRepository());
                    if (!Resolved.IsEmpty())
                    {
                        CachedCastSkillName = Resolved;
                        CastNameText->SetText(FText::FromString(Resolved));
                    }
                }
            }
        }
    }
    UpdateHP(CachedCurrentHP, CachedMaxHP);
}

#undef LOCTEXT_NAMESPACE
