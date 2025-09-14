#include "UI/SkillDragVisualWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

USkillDragVisualWidget::USkillDragVisualWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SkillIcon = nullptr;
    SkillNameText = nullptr;
    DragBorder = nullptr;
    DefaultSkillIcon = nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Constructor called"));
}

void USkillDragVisualWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Create components programmatically
    CreateDragVisualComponents();

    SetVisibility(ESlateVisibility::HitTestInvisible);
    
    // Set initial visual properties
    SetRenderOpacity(DragOpacity.A);
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: NativeConstruct called"));
    
    // Update visual display now that components are created
    UpdateVisualDisplay();
}

//void USkillDragVisualWidget::CreateDragVisualComponents()
//{
//    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Creating drag visual components"));
//    
//    // Create root border as the main container
//    DragBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragBorder"));
//    if (DragBorder)
//    {
//        // Set as root widget
//        WidgetTree->RootWidget = DragBorder;
//        
//        // Configure border appearance
//        FSlateBrush BorderBrush;
//        BorderBrush.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f)); // Semi-transparent dark background
//        BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
//        DragBorder->SetBrush(BorderBrush);
//        
//        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: DragBorder created and set as root"));
//    }
//    
//    // Create skill icon inside the border
//    SkillIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SkillIcon"));
//    if (SkillIcon && DragBorder)
//    {
//        DragBorder->AddChild(SkillIcon);
//        
//        // Configure icon size
//        SkillIcon->SetBrushSize(FVector2D(64.0f, 64.0f)); // Icon size
//        
//        UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: SkillIcon created and added to border"));
//    }
//    
//    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: All components created successfully"));
//}

void USkillDragVisualWidget::CreateDragVisualComponents()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragVisualWidget: Creating drag visual components"));

    // Root with fixed size so визуал точно не 0x0
    USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSize"));
    WidgetTree->RootWidget = RootSize;
    RootSize->SetWidthOverride(72.f);
    RootSize->SetHeightOverride(88.f); // место под текст под иконкой
    RootSize->SetMinDesiredWidth(72.f);
    RootSize->SetMinDesiredHeight(72.f);

    // Полупрозрачная подложка
    DragBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragBorder"));
    RootSize->AddChild(DragBorder);

    FSlateBrush BorderBrush;
    BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    BorderBrush.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
    DragBorder->SetBrush(BorderBrush);
    DragBorder->SetPadding(FMargin(4.f));
    DragBorder->SetHorizontalAlignment(HAlign_Center);
    DragBorder->SetVerticalAlignment(VAlign_Center);

    // Вертикальный стек: иконка + имя
    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox"));
    DragBorder->SetContent(VBox);

    // Иконка
    SkillIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SkillIcon"));
    UVerticalBoxSlot* IconSlot = VBox->AddChildToVerticalBox(SkillIcon);
    IconSlot->SetHorizontalAlignment(HAlign_Center);
    SkillIcon->SetDesiredSizeOverride(FVector2D(64.f, 64.f));
    SkillIcon->SetColorAndOpacity(DragOpacity); // альфа берётся из DragOpacity (0.8 по умолчанию)

    // Текст (по желанию можно скрывать)
    SkillNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SkillNameText"));
    UVerticalBoxSlot* NameSlot = VBox->AddChildToVerticalBox(SkillNameText);
    NameSlot->SetHorizontalAlignment(HAlign_Center);
    NameSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

    SkillNameText->SetText(FText::GetEmpty()); // заполнишь в UpdateVisualDisplay
    SkillNameText->SetShadowOffset(FVector2D(1.f, 1.f));
    SkillNameText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
    FSlateFontInfo Font = SkillNameText->GetFont();
    Font.Size = 10;
    SkillNameText->SetFont(Font);

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