#include "UI/SkillDragVisualWidget.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"

USkillDragVisualWidget::USkillDragVisualWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SkillIcon = nullptr;
    DefaultSkillIcon = nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Constructor called"));
}

void USkillDragVisualWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Build the widget tree if it hasn't been built already. When this widget is
    // used as a drag visual, BuildComponentsOnce() is called explicitly before
    // TakeWidget() snapshots the Slate widget, so by the time NativeConstruct runs
    // the tree already exists and we skip rebuilding. In the normal AddToViewport
    // path nobody calls BuildComponentsOnce() up front, so NativeConstruct does it.
    BuildComponentsOnce();

    SetVisibility(ESlateVisibility::HitTestInvisible);
    
    // Set initial visual properties
    SetRenderOpacity(DragOpacity.A);
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: NativeConstruct called (bComponentsCreated=%d)"), bComponentsCreated ? 1 : 0);
    
    // Update visual display now that components are created
    UpdateVisualDisplay();
}

void USkillDragVisualWidget::BuildComponentsOnce()
{
    if (bComponentsCreated)
    {
        return;
    }
    CreateDragVisualComponents();
    bComponentsCreated = true;
}

void USkillDragVisualWidget::CreateDragVisualComponents()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Creating drag visual components"));

    // Root with fixed size matching a skill slot (80x80). Square so the icon
    // sits under the cursor at the same scale as the source slot.
    USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSize"));
    WidgetTree->RootWidget = RootSize;
    RootSize->SetWidthOverride(80.f);
    RootSize->SetHeightOverride(80.f);
    RootSize->SetMinDesiredWidth(80.f);
    RootSize->SetMinDesiredHeight(80.f);

    // Single child: the skill icon, filling the root. No background border
    // (the previous UBorder drew a black translucent rounded box under the
    // icon) and no name text (it was always empty in the native path). This
    // yields a clean translucent icon under the cursor, nothing else.
    // USizeBox is a single-child container — the child fills it by default,
    // so no explicit slot alignment is needed.
    SkillIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SkillIcon"));
    RootSize->AddChild(SkillIcon);
    SkillIcon->SetDesiredSizeOverride(FVector2D(80.f, 80.f));
    SkillIcon->SetColorAndOpacity(DragOpacity); // 0.8 alpha — standard drag-visual feel

    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: All components created successfully"));
}

void USkillDragVisualWidget::SetSkillData(const FPlayerSkillData& SkillData)
{
    CurrentSkillData = SkillData;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: SetSkillData called for skill %s"), *SkillData.networkData.skillSlug);
    
    // Only update display if components are already created
    if (SkillIcon)
    {
        UpdateVisualDisplay();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Skill data set, but components not created yet. Will update in NativeConstruct."));
    }
}

void USkillDragVisualWidget::UpdateVisualDisplay()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: UpdateVisualDisplay called"));
    
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
                UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Set skill icon from texture"));
            }
        }
        
        // Try default icon
        if (!bIconSet && DefaultSkillIcon)
        {
            SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
            SkillIcon->SetColorAndOpacity(DragOpacity);
            bIconSet = true;
            UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Set default skill icon"));
        }
        
        // Fallback: create colored rectangle
        if (!bIconSet)
        {
            FSlateBrush IconBrush;
            IconBrush.TintColor = FSlateColor(FLinearColor(0.5f, 0.5f, 1.0f, DragOpacity.A)); // Blue tint
            IconBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
            SkillIcon->SetBrush(IconBrush);
            UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Set fallback colored icon"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: SkillIcon widget is NULL - cannot update display"));
    }

    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Visual display updated successfully"));
}