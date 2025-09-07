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

void USkillTooltipWidget::UpdateTooltipPosition(FVector2D ScreenPosition)
{
    if (!bIsVisible) return;

    // Apply offset
    FVector2D NewPosition = ScreenPosition + TooltipOffset;
    
    // Get viewport size to prevent tooltip from going off-screen
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 ViewportWidth = 0, ViewportHeight = 0;
        PC->GetViewportSize(ViewportWidth, ViewportHeight);

        // Force layout to get actual tooltip size
        ForceLayoutPrepass();
        FVector2D TooltipSize = GetDesiredSize();

        // Adjust position to keep tooltip on screen
        if (NewPosition.X + TooltipSize.X > ViewportWidth)
        {
            NewPosition.X = ScreenPosition.X - TooltipSize.X - FMath::Abs(TooltipOffset.X);
        }
        
        if (NewPosition.Y + TooltipSize.Y > ViewportHeight)
        {
            NewPosition.Y = ScreenPosition.Y - TooltipSize.Y - FMath::Abs(TooltipOffset.Y);
        }

        // Ensure tooltip doesn't go off the left or top edges
        NewPosition.X = FMath::Max(0.0f, NewPosition.X);
        NewPosition.Y = FMath::Max(0.0f, NewPosition.Y);
    }

    SetPositionInViewport(NewPosition, false);
}

void USkillTooltipWidget::ShowTooltip()
{
    if (bIsVisible) return;

    bIsVisible = true;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    
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

    // Update damage - показываем общую информацию, поскольку конкретных значений дамага нет
    if (DamageText)
    {
        FText DamageDisplayText = FText::FromString(TEXT("Damage: Variable"));
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
        FString LevelText = FString::Printf(TEXT("Level %d"), CurrentSkillData.networkData.skillLevel);
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

FText USkillTooltipWidget::FormatDamageText(int32 MinDamage, int32 MaxDamage) const
{
    if (MinDamage > 0 || MaxDamage > 0)
    {
        if (MinDamage == MaxDamage)
        {
            return FText::FromString(FString::Printf(TEXT("Damage: %d"), MinDamage));
        }
        else
        {
            return FText::FromString(FString::Printf(TEXT("Damage: %d - %d"), MinDamage, MaxDamage));
        }
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

    // Initialize effect type colors - используем только существующие типы
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