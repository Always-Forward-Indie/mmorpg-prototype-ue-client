#include "UI/SkillTooltipWidget.h"
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
    
    UE_LOG(LogTemp, Log, TEXT("SkillTooltipWidget: NativeConstruct completed"));
}

void USkillTooltipWidget::SetSkillData(const FPlayerSkillData& SkillData)
{
    CurrentSkillData = SkillData;
    UpdateTooltipContent();
    
    UE_LOG(LogTemp, Log, TEXT("SkillTooltipWidget: Set skill data for %s"), 
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
    
    UE_LOG(LogTemp, Log, TEXT("SkillTooltipWidget: Tooltip shown"));
}

void USkillTooltipWidget::HideTooltip()
{
    if (!bIsVisible) return;

    bIsVisible = false;
    SetVisibility(ESlateVisibility::Hidden);
    
    UE_LOG(LogTemp, Log, TEXT("SkillTooltipWidget: Tooltip hidden"));
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
    // Update skill name
    if (SkillNameText)
    {
        FText DisplayName = CurrentSkillData.definitionData.displayName;
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(CurrentSkillData.networkData.skillSlug);
        }
        SkillNameText->SetText(DisplayName);
        
        // Set name color based on skill school
        FLinearColor SchoolColor = GetSchoolColor(CurrentSkillData.definitionData.school);
        SkillNameText->SetColorAndOpacity(SchoolColor);
    }

    // Update skill school
    if (SkillSchoolText)
    {
        FString SchoolName = UEnum::GetValueAsString(CurrentSkillData.definitionData.school);
        // Remove the enum prefix (e.g., "ESkillSchool::Fire" -> "Fire")
        SchoolName = SchoolName.Replace(TEXT("ESkillSchool::"), TEXT(""));
        SkillSchoolText->SetText(FText::FromString(SchoolName));
        
        FLinearColor SchoolColor = GetSchoolColor(CurrentSkillData.definitionData.school);
        SkillSchoolText->SetColorAndOpacity(SchoolColor);
    }
}

void USkillTooltipWidget::UpdateSkillDescription()
{
    if (SkillDescriptionText)
    {
        SkillDescriptionText->SetText(CurrentSkillData.definitionData.description);
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
            FString RangeString = FString::Printf(TEXT("Range: %.1f"), CurrentSkillData.networkData.maxRange);
            RangeText->SetText(FText::FromString(RangeString));
        }
        else
        {
            RangeText->SetText(FText::FromString(TEXT("Melee")));
        }
    }

    // Update effect type
    if (EffectTypeText)
    {
        FString EffectTypeName = UEnum::GetValueAsString(CurrentSkillData.definitionData.effectType);
        EffectTypeName = EffectTypeName.Replace(TEXT("ESkillEffectType::"), TEXT(""));
        EffectTypeText->SetText(FText::FromString(EffectTypeName));
        
        FLinearColor EffectTypeColor = GetEffectTypeColor(CurrentSkillData.definitionData.effectType);
        EffectTypeText->SetColorAndOpacity(EffectTypeColor);
    }
}

void USkillTooltipWidget::UpdateSkillLevelInfo()
{
    if (SkillLevelText)
    {
        FString LevelText = FString::Printf(TEXT("lvl %d"), CurrentSkillData.networkData.skillLevel);
        SkillLevelText->SetText(FText::FromString(LevelText));
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
        return FText::FromString(TEXT("Instant"));
    }
    else if (CooldownSeconds < 60.0f)
    {
        return FText::FromString(FString::Printf(TEXT("Cooldown: %.1fs"), CooldownSeconds));
    }
    else
    {
        int32 Minutes = static_cast<int32>(CooldownSeconds / 60.0f);
        int32 Seconds = static_cast<int32>(CooldownSeconds) % 60;
        return FText::FromString(FString::Printf(TEXT("Cooldown: %dm %ds"), Minutes, Seconds));
    }
}

FText USkillTooltipWidget::FormatManaCostText(int32 ManaCost) const
{
    if (ManaCost > 0)
    {
        return FText::FromString(FString::Printf(TEXT("Mana Cost: %d"), ManaCost));
    }
    else
    {
        return FText::FromString(TEXT("No Mana Cost"));
    }
}

FText USkillTooltipWidget::FormatDamageText(int32 Flat, float Coeff) const
{
    if (Flat > 0 && Coeff > 0)
    {
        return FText::FromString(FString::Printf(TEXT("Damage: Flat: %d * Coeff: %.2f + Stats Value"), Flat, static_cast<double>(Coeff)));
    }
    else
    {
        return FText::FromString(TEXT("No Damage"));
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