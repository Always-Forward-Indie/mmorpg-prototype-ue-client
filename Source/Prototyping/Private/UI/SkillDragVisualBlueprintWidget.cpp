#include "UI/SkillDragVisualBlueprintWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

USkillDragVisualBlueprintWidget::USkillDragVisualBlueprintWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SkillIcon = nullptr;
    SkillNameText = nullptr;
    DragBorder = nullptr;
    DefaultSkillIcon = nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: Constructor called"));
}

void USkillDragVisualBlueprintWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetVisibility(ESlateVisibility::HitTestInvisible);
    
    // Set initial visual properties
    SetRenderOpacity(DragOpacity.A);
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: NativeConstruct called"));
    
    // Verify that components are bound
    if (!SkillIcon)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDragVisualBlueprintWidget: SkillIcon not bound! Make sure to bind it in Blueprint."));
    }
    if (!SkillNameText)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDragVisualBlueprintWidget: SkillNameText not bound! Make sure to bind it in Blueprint."));
    }
    if (!DragBorder)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDragVisualBlueprintWidget: DragBorder not bound! Make sure to bind it in Blueprint."));
    }
    
    // Update visual display if we have skill data
    if (!CurrentSkillData.networkData.skillSlug.IsEmpty())
    {
        UpdateVisualDisplay();
    }
}

void USkillDragVisualBlueprintWidget::SetSkillData(const FPlayerSkillData& SkillData)
{
    CurrentSkillData = SkillData;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: SetSkillData called for skill %s"), *SkillData.networkData.skillSlug);
    
    // Update visual display if components are available
    if (SkillIcon)
    {
        UpdateVisualDisplay();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: Skill data set, but components not ready yet. Will update in NativeConstruct."));
    }
    
    // Call Blueprint event
    OnSkillDataSet(SkillData);
}

void USkillDragVisualBlueprintWidget::UpdateVisualDisplay()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: UpdateVisualDisplay called"));
    
    // Update skill icon
    if (SkillIcon)
    {
        bool bIconSet = false;
        
        // Try to load skill icon
        if (CurrentSkillData.definitionData.skillIcon.IsValid())
        {
            if (UTexture2D* IconTexture = CurrentSkillData.definitionData.skillIcon.LoadSynchronous())
            {
                SkillIcon->SetBrushFromTexture(IconTexture);
                SkillIcon->SetColorAndOpacity(DragOpacity);
                bIconSet = true;
                UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: Set skill icon from texture"));
            }
        }
        
        // Try default icon
        if (!bIconSet && DefaultSkillIcon)
        {
            SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
            SkillIcon->SetColorAndOpacity(DragOpacity);
            bIconSet = true;
            UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: Set default skill icon"));
        }
        
        // Fallback: create colored rectangle based on skill school
        if (!bIconSet)
        {
            FSlateBrush IconBrush;
            FLinearColor SchoolColor = GetSchoolColor(CurrentSkillData.definitionData.school);
            IconBrush.TintColor = FSlateColor(SchoolColor);
            IconBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
            SkillIcon->SetBrush(IconBrush);
            UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: Set fallback colored icon"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: SkillIcon widget is NULL - cannot update display"));
    }

    // Update skill name
    if (SkillNameText)
    {
        FText DisplayName = CurrentSkillData.definitionData.displayName;
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(CurrentSkillData.networkData.skillSlug);
        }
        SkillNameText->SetText(DisplayName);
        SkillNameText->SetColorAndOpacity(DragOpacity);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: SkillNameText widget is NULL - cannot update text"));
    }

    // Update border
    if (DragBorder)
    {
        DragBorder->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, DragOpacity.A * 0.5f));
    }

    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualBlueprintWidget: Visual display updated successfully"));
}

FLinearColor USkillDragVisualBlueprintWidget::GetSchoolColor(ESkillSchool School) const
{
    switch (School)
    {
        case ESkillSchool::Physical: return FLinearColor(0.8f, 0.4f, 0.2f, DragOpacity.A); // Brown
        case ESkillSchool::Fire: return FLinearColor(1.0f, 0.3f, 0.0f, DragOpacity.A); // Red
        case ESkillSchool::Ice: return FLinearColor(0.4f, 0.8f, 1.0f, DragOpacity.A); // Light Blue
        case ESkillSchool::Nature: return FLinearColor(0.2f, 0.8f, 0.2f, DragOpacity.A); // Green
        case ESkillSchool::Arcane: return FLinearColor(0.6f, 0.2f, 1.0f, DragOpacity.A); // Purple
        case ESkillSchool::Shadow: return FLinearColor(0.3f, 0.1f, 0.5f, DragOpacity.A); // Dark Purple
        case ESkillSchool::Holy: return FLinearColor(1.0f, 1.0f, 0.3f, DragOpacity.A); // Golden
        default: return FLinearColor(0.5f, 0.5f, 0.5f, DragOpacity.A); // Gray
    }
}