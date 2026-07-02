#include "UI/SkillDragDropOperation.h"
#include "UI/AvailableSkillsWidget.h"  // This contains USkillItemWidget definition
#include "UI/SkillSlotWidget.h"
#include "UI/SkillDragVisualWidget.h"
#include "UI/SkillDragVisualBlueprintWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

USkillDragDropOperation::USkillDragDropOperation()
{
    SourceWidget = nullptr;
    SourceSlotWidget = nullptr;
    SourceSlotIndex = -1;
    Pivot = EDragPivot::MouseDown;
    Offset = FVector2D::ZeroVector;

    // Default drag visual: the C++ USkillDragVisualWidget builds its components
    // programmatically in NativeConstruct, so no Blueprint is required. This must
    // NEVER be null + use the source widget as the visual — reparenting the source
    // corrupts its container layout and hit-testing for the whole drag (root cause
    // of the slot-to-slot drag flicker / drop-on-self bug). Callers may still
    // override DragVisualWidgetClass with a Blueprint subclass.
    DragVisualWidgetClass = USkillDragVisualWidget::StaticClass();

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

    // If custom creation failed, proceed with no drag visual. Using the source
    // widget as DefaultDragVisual is forbidden — UMG reparents it out of its
    // container for the duration of the drag, which corrupts the layout and
    // hit-testing of the source's siblings (root cause of slot drag flicker).
    DefaultDragVisual = nullptr;
    
    // Set visual properties for better drag experience
    Pivot = EDragPivot::MouseDown;
    Offset = FVector2D::ZeroVector;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: No drag visual (proceeding without one)"));
}

UUserWidget* USkillDragDropOperation::CreateDragVisualWidget()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: CreateDragVisualWidget called"));

    if (!DragVisualWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: No DragVisualWidgetClass"));
        return nullptr;
    }

    // Determine the owning player from either source
    APlayerController* OwningPlayer = nullptr;
    if (SourceWidget)
    {
        OwningPlayer = SourceWidget->GetOwningPlayer();
    }
    else if (SourceSlotWidget)
    {
        OwningPlayer = SourceSlotWidget->GetOwningPlayer();
    }

    if (!OwningPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: No OwningPlayer available"));
        return nullptr;
    }

    // Создаём у того же OwningPlayer
    UUserWidget* DragVisual = CreateWidget<UUserWidget>(
        OwningPlayer,
        DragVisualWidgetClass
    );
    if (!DragVisual)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDragDropOperation: CreateWidget failed"));
        return nullptr;
    }

    // Typed path: the dedicated drag-visual widget classes own their layout and
    // build components in NativeConstruct. Hand them the skill data via
    // SetSkillData() — they store it and defer rendering to NativeConstruct,
    // which runs after components exist. This fixes the ordering bug where
    // GetWidgetFromName() could not find SkillIcon before NativeConstruct.
    if (USkillDragVisualWidget* NativeVisual = Cast<USkillDragVisualWidget>(DragVisual))
    {
        // Build the widget tree BEFORE the drag system calls TakeWidget() to
        // snapshot the Slate widget. USkillDragVisualWidget builds its tree
        // programmatically in NativeConstruct, but in a drag-drop context
        // TakeWidget() snapshots the (empty) tree before NativeConstruct fires,
        // so nothing would render under the cursor. Forcing the build here
        // ensures the tree exists at snapshot time; the internal guard prevents
        // NativeConstruct from rebuilding later.
        NativeVisual->BuildComponentsOnce();
        NativeVisual->SetSkillData(SkillData);
        NativeVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Configured USkillDragVisualWidget drag visual"));
        return NativeVisual;
    }
    if (USkillDragVisualBlueprintWidget* BpVisual = Cast<USkillDragVisualBlueprintWidget>(DragVisual))
    {
        BpVisual->SetSkillData(SkillData);
        BpVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
        UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Configured USkillDragVisualBlueprintWidget drag visual"));
        return BpVisual;
    }

    // Generic fallback for arbitrary widget classes: configure known components
    // by name. Only meaningful once the widget's own NativeConstruct has bound
    // them, so this path is best-effort.
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
    if (UTextBlock* SkillNameText = Cast<UTextBlock>(DragVisual->GetWidgetFromName(TEXT("SkillNameText"))))
    {
        FText DisplayName = SkillData.definitionData.displayName;
        if (DisplayName.IsEmpty())
        {
            DisplayName = FText::FromString(SkillData.networkData.skillSlug);
        }
        SkillNameText->SetText(DisplayName);
    }
    if (UBorder* Border = Cast<UBorder>(DragVisual->GetWidgetFromName(TEXT("DragBorder"))))
    {
        Border->SetBrushColor(FLinearColor(1, 1, 1, 0.8f));
    }
    
    DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
    UE_LOG(LogTemp, Warning, TEXT("SkillDragDropOperation: Configured drag visual widget (generic path)"));

    return DragVisual;
}