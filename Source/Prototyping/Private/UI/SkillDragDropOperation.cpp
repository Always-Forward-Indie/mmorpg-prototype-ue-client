#include "UI/SkillDragDropOperation.h"
#include "UI/AvailableSkillsWidget.h"  // This contains USkillItemWidget definition
#include "UI/SkillDragVisualWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

USkillDragDropOperation::USkillDragDropOperation()
{
    SourceWidget = nullptr;
    Pivot = EDragPivot::MouseDown;
    Offset = FVector2D::ZeroVector;
    
    // DragVisualWidgetClass будет установлен из USkillItemWidget
    DragVisualWidgetClass = nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Constructor called"));
}

void USkillDragDropOperation::SetSkillData(const FPlayerSkillData& InSkillData, USkillItemWidget* InSourceWidget)
{
    SkillData = InSkillData;
    SourceWidget = InSourceWidget;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: SetSkillData called for skill %s"), *SkillData.networkData.skillSlug);
}

void USkillDragDropOperation::CreateDefaultDragVisual()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: CreateDefaultDragVisual called"));
    
    if (!SourceWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: No source widget available for drag visual"));
        return;
    }

    // Try to create custom drag visual widget first
    UUserWidget* CustomDragVisual = CreateDragVisualWidget();
    if (CustomDragVisual)
    {
        DefaultDragVisual = CustomDragVisual;
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Created custom drag visual"));
        return;
    }

    // If custom creation failed, use the source widget as drag visual
    DefaultDragVisual = SourceWidget;
    
    // Set visual properties for better drag experience
    Pivot = EDragPivot::MouseDown;
    Offset = FVector2D::ZeroVector;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Using source widget as drag visual"));
}

UUserWidget* USkillDragDropOperation::CreateDragVisualWidget()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: CreateDragVisualWidget called"));

    if (!DragVisualWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: No DragVisualWidgetClass"));
        return nullptr;
    }
    if (!SourceWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: No SourceWidget"));
        return nullptr;
    }

    // Создаём у того же OwningPlayer
    UUserWidget* DragVisual = CreateWidget<UUserWidget>(
        SourceWidget->GetOwningPlayer(),
        DragVisualWidgetClass
    );
    if (!DragVisual)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDragDropOperation: CreateWidget failed"));
        return nullptr;
    }

    // Всегда используем generic настройку через компоненты по именам
    // Это работает как с USkillDragVisualWidget, так и с Blueprint виджетами
    
    // Настройка иконки
    if (UImage* SkillIcon = Cast<UImage>(DragVisual->GetWidgetFromName(TEXT("SkillIcon"))))
    {
        if (SkillData.definitionData.skillIcon.IsValid())
        {
            if (UTexture2D* IconTexture = SkillData.definitionData.skillIcon.LoadSynchronous())
            {
                SkillIcon->SetBrushFromTexture(IconTexture);
            }
        }
    }
    
    // Настройка текста
    if (UTextBlock* SkillNameText = Cast<UTextBlock>(DragVisual->GetWidgetFromName(TEXT("SkillNameText"))))
    {
        FText DisplayName = SkillData.definitionData.displayName;
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(SkillData.networkData.skillSlug);
        }
        SkillNameText->SetText(DisplayName);
    }
    
    // Настройка бордера
    if (UBorder* Border = Cast<UBorder>(DragVisual->GetWidgetFromName(TEXT("DragBorder"))))
    {
        Border->SetBrushColor(FLinearColor(1, 1, 1, 0.8f));
    }
    
    DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Configured drag visual widget"));

    return DragVisual;
}