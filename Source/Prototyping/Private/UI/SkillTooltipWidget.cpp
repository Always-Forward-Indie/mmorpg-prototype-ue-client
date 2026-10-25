#include "UI/SkillTooltipWidget.h"
#include "Prototyping.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// S5: tooltip chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "SkillTooltip"

// File-local enum display names (defined below, used in header/stats).
static FText GetSkillSchoolDisplayName(ESkillSchool School);
static FText GetEffectTypeDisplayName(ESkillEffectType EffectType);

USkillTooltipWidget::USkillTooltipWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsVisible = false;
    DefaultSkillIcon = nullptr;
}

void USkillTooltipWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize colors
    InitializeColors();

    // Set initial visibility
    SetVisibility(ESlateVisibility::Hidden);
    
    UE_LOG(LogGameplay, Verbose, TEXT("SkillTooltipWidget: NativeConstruct completed"));
}

void USkillTooltipWidget::SetSkillData(const FPlayerSkillData& SkillData)
{
    CurrentSkillData = SkillData;
    UpdateTooltipContent();
    
    UE_LOG(LogGameplay, Verbose, TEXT("SkillTooltipWidget: Set skill data for %s"), 
        *SkillData.networkData.skillSlug);
}

void USkillTooltipWidget::UpdateTooltipPosition(FVector2D /*ScreenPosition*/)
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // Use viewport-space mouse position (matches SetPositionInViewport coordinate space)
    const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(PC);

    // Force layout to get accurate size
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceLayoutPrepass();

    FVector2D Size = GetCachedGeometry().GetLocalSize();
    if (Size.X <= 1.f || Size.Y <= 1.f)
        Size = GetDesiredSize();
    if (Size.IsZero())
        Size = FVector2D(300, 200);

    int32 Wpx = 0, Hpx = 0;
    PC->GetViewportSize(Wpx, Hpx);
    const FVector2D View(Wpx, Hpx);

    const float SafePad = 20.f;
    const FVector2D BaseOffset = TooltipOffset + FVector2D(SafePad, SafePad);

    FVector2D Pos = MousePos + BaseOffset;

    const float EarlyPad = SafePad + 50.f;
    if (Pos.X + Size.X > View.X - EarlyPad)
        Pos.X = MousePos.X - Size.X - FMath::Abs(TooltipOffset.X) - SafePad;
    if (Pos.Y + Size.Y > View.Y - EarlyPad)
        Pos.Y = MousePos.Y - Size.Y - FMath::Abs(TooltipOffset.Y) - SafePad;

    Pos.X = FMath::Clamp(Pos.X, 0.f, View.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, View.Y - Size.Y);

    SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
    SetPositionInViewport(Pos, false);
}

void USkillTooltipWidget::ShowTooltip()
{
    if (bIsVisible) return;

    bIsVisible = true;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceLayoutPrepass();

    // Simple fade in animation (can be expanded with UMG animations)
    SetRenderOpacity(0.0f);
    
    // You can add UMG animation here or use a simple opacity tween
    SetRenderOpacity(1.0f); // For now, just show immediately
    
    UE_LOG(LogGameplay, Verbose, TEXT("SkillTooltipWidget: Tooltip shown"));
}

void USkillTooltipWidget::HideTooltip()
{
    if (!bIsVisible) return;

    bIsVisible = false;
    SetVisibility(ESlateVisibility::Hidden);
    
    UE_LOG(LogGameplay, Verbose, TEXT("SkillTooltipWidget: Tooltip hidden"));
}

void USkillTooltipWidget::UpdateTooltipContent()
{
    UpdateSkillHeader();
    UpdateSkillDescription();
    UpdateSkillStats();
    UpdateSkillLevelInfo();
    LoadSkillIcon();
}

void USkillTooltipWidget::UpdateSkillHeader()
{
    // Update skill name: S3c codebook table row wins when filled, repo
    // definition second, slug last (repo is single-language, table is RU/EN).
    if (SkillNameText)
    {
        const FString& Slug = CurrentSkillData.networkData.skillSlug;
        FText DisplayName = CurrentSkillData.definitionData.displayName;
        if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
            DisplayName = ULocalizationSubsystem::PickTableOrServer(
                Loc->GetSkillDisplayName(Slug), Slug, CurrentSkillData.definitionData.displayName.ToString());
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(Slug);
        }
        SkillNameText->SetText(DisplayName);
        
        // Set name color based on skill school
        FLinearColor SchoolColor = GetSchoolColor(CurrentSkillData.definitionData.school);
        SkillNameText->SetColorAndOpacity(SchoolColor);
    }

    // Update skill school
    if (SkillSchoolText)
    {
        SkillSchoolText->SetText(GetSkillSchoolDisplayName(CurrentSkillData.definitionData.school));
        
        FLinearColor SchoolColor = GetSchoolColor(CurrentSkillData.definitionData.school);
        SkillSchoolText->SetColorAndOpacity(SchoolColor);
    }
}

void USkillTooltipWidget::UpdateSkillDescription()
{
    if (SkillDescriptionText)
    {
        // S3c: localized description first, repo second.
        FText Desc = CurrentSkillData.definitionData.description;
        if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
        {
            const FText LocDesc = Loc->GetSkillDescription(CurrentSkillData.networkData.skillSlug);
            if (!LocDesc.IsEmpty())
                Desc = LocDesc;
        }
        SkillDescriptionText->SetText(Desc);
    }
}

void USkillTooltipWidget::UpdateSkillStats()
{
    // Update cooldown
    if (CooldownText)
    {
        FText CooldownDisplayText = FormatCooldownText(CurrentSkillData.networkData.cooldownMs);
        CooldownText->SetText(CooldownDisplayText);
    }

    // Update mana cost
    if (ManaCostText)
    {
        FText ManaCostDisplayText = FormatManaCostText(CurrentSkillData.networkData.costMp);
        ManaCostText->SetText(ManaCostDisplayText);
    }

    // Update damage
    if (DamageText)
    {
        FText DamageDisplayText = FormatDamageText(
            CurrentSkillData.networkData.flatAdd,
            CurrentSkillData.networkData.coeff);
        DamageText->SetText(DamageDisplayText);
    }

    // Update range
    if (RangeText)
    {
        if (CurrentSkillData.networkData.maxRange > 0.0f)
        {
            RangeText->SetText(FText::Format(LOCTEXT("RangeValue", "Range: {0}"),
                FText::AsNumber(CurrentSkillData.networkData.maxRange)));
        }
        else
        {
            RangeText->SetText(LOCTEXT("Melee", "Melee"));
        }
    }

    // Update effect type
    if (EffectTypeText)
    {
        EffectTypeText->SetText(GetEffectTypeDisplayName(CurrentSkillData.definitionData.effectType));
        
        FLinearColor EffectTypeColor = GetEffectTypeColor(CurrentSkillData.definitionData.effectType);
        EffectTypeText->SetColorAndOpacity(EffectTypeColor);
    }
}

// S5: enum display names gathered via LOCTEXT (file-local: tooltip-only).
static FText GetSkillSchoolDisplayName(ESkillSchool School)
{
    switch (School)
    {
    case ESkillSchool::Physical: return LOCTEXT("SchoolPhysical", "Physical");
    case ESkillSchool::Fire:     return LOCTEXT("SchoolFire", "Fire");
    case ESkillSchool::Ice:      return LOCTEXT("SchoolIce", "Ice");
    case ESkillSchool::Nature:   return LOCTEXT("SchoolNature", "Nature");
    case ESkillSchool::Arcane:   return LOCTEXT("SchoolArcane", "Arcane");
    case ESkillSchool::Shadow:   return LOCTEXT("SchoolShadow", "Shadow");
    case ESkillSchool::Holy:     return LOCTEXT("SchoolHoly", "Holy");
    default:                     return LOCTEXT("SchoolNone", "None");
    }
}

static FText GetEffectTypeDisplayName(ESkillEffectType EffectType)
{
    switch (EffectType)
    {
    case ESkillEffectType::Damage:  return LOCTEXT("EffectDamage", "Damage");
    case ESkillEffectType::Healing: return LOCTEXT("EffectHealing", "Healing");
    case ESkillEffectType::Buff:    return LOCTEXT("EffectBuff", "Buff");
    case ESkillEffectType::Debuff:  return LOCTEXT("EffectDebuff", "Debuff");
    case ESkillEffectType::Resource: return LOCTEXT("EffectResource", "Resource");
    default:                        return LOCTEXT("EffectNone", "None");
    }
}

void USkillTooltipWidget::UpdateSkillLevelInfo()
{
    if (SkillLevelText)
    {
        SkillLevelText->SetText(FText::Format(LOCTEXT("LevelValue", "lvl {0}"),
            FText::AsNumber(CurrentSkillData.networkData.skillLevel)));
    }
}

FLinearColor USkillTooltipWidget::GetSchoolColor(ESkillSchool School) const
{
    if (const FLinearColor* Color = SchoolColors.Find(School))
    {
        return *Color;
    }
    return FLinearColor::White; // Default color
}

FLinearColor USkillTooltipWidget::GetEffectTypeColor(ESkillEffectType EffectType) const
{
    if (const FLinearColor* Color = EffectTypeColors.Find(EffectType))
    {
        return *Color;
    }
    return FLinearColor::White; // Default color
}

FText USkillTooltipWidget::FormatCooldownText(float CooldownMs) const
{
    float CooldownSeconds = CooldownMs / 1000.0f;
    
    if (CooldownSeconds < 1.0f)
    {
        return LOCTEXT("Instant", "Instant");
    }
    else if (CooldownSeconds < 60.0f)
    {
        return FText::Format(LOCTEXT("CooldownSecs", "Cooldown: {0}s"), FText::AsNumber(CooldownSeconds));
    }
    else
    {
        int32 Minutes = static_cast<int32>(CooldownSeconds / 60.0f);
        int32 Seconds = static_cast<int32>(CooldownSeconds) % 60;
        return FText::Format(LOCTEXT("CooldownMinSecs", "Cooldown: {0}m {1}s"), Minutes, Seconds);
    }
}

FText USkillTooltipWidget::FormatManaCostText(int32 ManaCost) const
{
    if (ManaCost > 0)
    {
        return FText::Format(LOCTEXT("ManaCostValue", "Mana Cost: {0}"), FText::AsNumber(ManaCost));
    }
    else
    {
        return LOCTEXT("NoManaCost", "No Mana Cost");
    }
}

FText USkillTooltipWidget::FormatDamageText(int32 Flat, float Coeff) const
{
    if (Flat > 0 && Coeff > 0)
    {
        return FText::Format(LOCTEXT("DamageFormula", "Damage: Flat: {0} * Coeff: {1} + Stats Value"),
            FText::AsNumber(Flat), FText::AsNumber(Coeff));
    }
    else
    {
        return LOCTEXT("NoDamage", "No Damage");
    }
}

void USkillTooltipWidget::InitializeColors()
{
    // Initialize school colors
    SchoolColors.Empty();
    SchoolColors.Add(ESkillSchool::Physical, FLinearColor(0.8f, 0.4f, 0.2f, 1.0f)); // Brown
    SchoolColors.Add(ESkillSchool::Fire, FLinearColor(1.0f, 0.3f, 0.0f, 1.0f)); // Red
    SchoolColors.Add(ESkillSchool::Ice, FLinearColor(0.4f, 0.8f, 1.0f, 1.0f)); // Light Blue
    SchoolColors.Add(ESkillSchool::Nature, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f)); // Green
    SchoolColors.Add(ESkillSchool::Arcane, FLinearColor(0.6f, 0.2f, 1.0f, 1.0f)); // Purple
    SchoolColors.Add(ESkillSchool::Shadow, FLinearColor(0.3f, 0.1f, 0.5f, 1.0f)); // Dark Purple
    SchoolColors.Add(ESkillSchool::Holy, FLinearColor(1.0f, 1.0f, 0.3f, 1.0f)); // Golden

    // Initialize effect type colors - ���������� ������ ������������ ����
    EffectTypeColors.Empty();
    EffectTypeColors.Add(ESkillEffectType::Damage, FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)); // Red
    EffectTypeColors.Add(ESkillEffectType::Healing, FLinearColor(0.2f, 1.0f, 0.2f, 1.0f)); // Green
    EffectTypeColors.Add(ESkillEffectType::Buff, FLinearColor(0.2f, 0.6f, 1.0f, 1.0f)); // Blue
    EffectTypeColors.Add(ESkillEffectType::Debuff, FLinearColor(1.0f, 0.6f, 0.0f, 1.0f)); // Orange
    EffectTypeColors.Add(ESkillEffectType::Resource, FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)); // Gray
}

void USkillTooltipWidget::LoadSkillIcon()
{
    if (!SkillIcon) return;

    bool bIconSet = false;

    if (CurrentSkillData.definitionData.skillIcon.IsValid())
    {
        // Use async loading for better performance
        AsyncLoad(CurrentSkillData.definitionData.skillIcon.ToSoftObjectPath(), 
            FSimpleDelegate::CreateUObject(this, &USkillTooltipWidget::SetIconTexture, 
                CurrentSkillData.definitionData.skillIcon.LoadSynchronous()));
        bIconSet = true;
    }

    if (!bIconSet)
    {
        SetDefaultIcon();
    }
}

void USkillTooltipWidget::SetIconTexture(UTexture2D* Texture)
{
    if (SkillIcon && Texture)
    {
        SkillIcon->SetBrushFromTexture(Texture);
    }
}

void USkillTooltipWidget::SetDefaultIcon()
{
    if (SkillIcon && DefaultSkillIcon)
    {
        SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
    }
}

void USkillTooltipWidget::AsyncLoad(const FSoftObjectPath& Path, FSimpleDelegate Callback)
{
    if (Path.IsValid())
    {
        FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
        StreamableHandle = StreamableManager.RequestAsyncLoad(Path, Callback);
    }
}

#undef LOCTEXT_NAMESPACE