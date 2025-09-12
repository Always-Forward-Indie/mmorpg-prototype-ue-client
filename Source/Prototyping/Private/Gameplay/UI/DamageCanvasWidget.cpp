#include "Gameplay/UI/DamageCanvasWidget.h"
#include "Components/CanvasPanel.h"

void UDamageCanvasWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    InitializeDamageCanvas();
    
    UE_LOG(LogTemp, Warning, TEXT("DamageCanvasWidget: Constructed and initialized"));
}

void UDamageCanvasWidget::NativeDestruct()
{
    Super::NativeDestruct();
    
    UE_LOG(LogTemp, Warning, TEXT("DamageCanvasWidget: Destructed"));
}

void UDamageCanvasWidget::InitializeDamageCanvas()
{
    if (!DamageCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("DamageCanvasWidget: DamageCanvas is not bound in Blueprint"));
        return;
    }

    // Ensure the canvas is properly set up for damage effects
    DamageCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    
    UE_LOG(LogTemp, Warning, TEXT("DamageCanvasWidget: Damage canvas initialized successfully"));
}

void UDamageCanvasWidget::ClearAllDamageEffects()
{
    if (!DamageCanvas)
    {
        UE_LOG(LogTemp, Warning, TEXT("DamageCanvasWidget: Cannot clear effects - DamageCanvas is null"));
        return;
    }

    // Clear all child widgets from the damage canvas
    DamageCanvas->ClearChildren();
    
    UE_LOG(LogTemp, Log, TEXT("DamageCanvasWidget: All damage effects cleared"));
}