// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/MessageBoxPopup.h"


// Конструктор – делаем виджет фокусируемым
UMessageBoxPopup::UMessageBoxPopup(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void UMessageBoxPopup::NativeConstruct()
{
    Super::NativeConstruct();

    // Получаем размеры экрана (в пикселях)
    FVector2D ViewportSize(0.f, 0.f);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }

    // Центр экрана
    FVector2D ScreenCenter(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);


    //SetAlignmentInViewport(FVector2D(0.f, 0.f));
    //Смещаем для центрирования виджета
    SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

    // Добавили виджет на экран, сразу ставим позицию
    SetPositionInViewport(ScreenCenter, true);

    CurrentPos = ScreenCenter;

    if (LeftButton)
    {
        // Привязываем обработчик для кнопки "Left"
        LeftButton->OnClicked.AddDynamic(this, &UMessageBoxPopup::HandleLeftButtonClicked);
    }

    if (RightButton)
    {
        // Привязываем обработчик для кнопки "Right"
        RightButton->OnClicked.AddDynamic(this, &UMessageBoxPopup::HandleRightButtonClicked);
    }
}

void UMessageBoxPopup::SetupMessageBox(const FText& InTitle, const FText& InMessage, const FText& InLeftButtonLabel, const FText& InRightButtonLabel)
{
    if (TitleText)
    {
        TitleText->SetText(InTitle);
    }

    if (MessageText)
    {
        MessageText->SetText(InMessage);
    }

    // Предполагается, что текст для кнопки является первым дочерним элементом кнопки.
    if (LeftButton && LeftButton->GetChildAt(0))
    {
        if (UTextBlock* LeftButtonText = Cast<UTextBlock>(LeftButton->GetChildAt(0)))
        {
            LeftButtonText->SetText(InLeftButtonLabel);
        }
    }

    if (RightButton && RightButton->GetChildAt(0))
    {
        if (UTextBlock* RightButtonText = Cast<UTextBlock>(RightButton->GetChildAt(0)))
        {
            RightButtonText->SetText(InRightButtonLabel);
        }
    }
}



// --------------------- Перетаскивание ---------------------

FReply UMessageBoxPopup::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // Начинаем драг, если ЛКМ
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = true;

        // Берём позицию курсора (экран / физические пиксели)
        FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();

        // DragOffset = (где нажали) - (где сейчас находится верхний левый угол)
        DragOffset = MousePos - CurrentPos;

        // Захватываем мышь, чтобы продолжать получать MouseMove, даже если курсор выйдет за границы виджета
        FEventReply EventReply = UWidgetBlueprintLibrary::Handled();
        EventReply = UWidgetBlueprintLibrary::CaptureMouse(EventReply, this);

        return EventReply.NativeReply;
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMessageBoxPopup::NativeOnMouseMove(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // Если мы в режиме перетаскивания
    if (bDragging)
    {
        FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();
        FVector2D NewPos = MousePos - DragOffset; // смещаем так, чтобы курсор остался на той же точке виджета

        // Получаем размер окна
        FVector2D ViewportSize(0.f, 0.f);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(ViewportSize);
        }

        // Узнаём размеры виджета (обычно "логические" пиксели, но мы используем bRemoveDPIScale = true)
        FVector2D WidgetSize = GetDesiredSize();

        // Ограничиваем, чтобы не выходить за край
        float HalfW = WidgetSize.X * 0.5f;
        float HalfH = WidgetSize.Y * 0.5f;

        NewPos.X = FMath::Clamp(NewPos.X, HalfW, ViewportSize.X - HalfW);
        NewPos.Y = FMath::Clamp(NewPos.Y, HalfH, ViewportSize.Y - HalfH);

        // Сохраняем новую позицию
        CurrentPos = NewPos;

        // Ставим виджет (true -> координаты считаются физическими пикселями)
        SetPositionInViewport(CurrentPos, true);

        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UMessageBoxPopup::NativeOnMouseButtonUp(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // Если мы перетаскивали и отпустили ЛКМ, заканчиваем драг
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;

        // Освобождаем захват
        FEventReply EventReply = UWidgetBlueprintLibrary::Handled();
        EventReply = UWidgetBlueprintLibrary::ReleaseMouseCapture(EventReply);

        return EventReply.NativeReply;
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UMessageBoxPopup::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);

    // Если потеряли захват (например, кликнули за пределами окна) – сбрасываем флаг
    bDragging = false;
}





void UMessageBoxPopup::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    
}


void UMessageBoxPopup::HandleLeftButtonClicked()
{
    OnLeftButtonClicked.Broadcast();
}

void UMessageBoxPopup::HandleRightButtonClicked()
{
    OnRightButtonClicked.Broadcast();
}