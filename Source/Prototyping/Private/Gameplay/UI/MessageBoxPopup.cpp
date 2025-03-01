// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/MessageBoxPopup.h"


// ����������� � ������ ������ ������������
UMessageBoxPopup::UMessageBoxPopup(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void UMessageBoxPopup::NativeConstruct()
{
    Super::NativeConstruct();

    // �������� ������� ������ (� ��������)
    FVector2D ViewportSize(0.f, 0.f);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }

    // ����� ������
    FVector2D ScreenCenter(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);


    //SetAlignmentInViewport(FVector2D(0.f, 0.f));
    //������� ��� ������������� �������
    SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

    // �������� ������ �� �����, ����� ������ �������
    SetPositionInViewport(ScreenCenter, true);

    CurrentPos = ScreenCenter;

    if (LeftButton)
    {
        // ����������� ���������� ��� ������ "Left"
        LeftButton->OnClicked.AddDynamic(this, &UMessageBoxPopup::HandleLeftButtonClicked);
    }

    if (RightButton)
    {
        // ����������� ���������� ��� ������ "Right"
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

    // ��������������, ��� ����� ��� ������ �������� ������ �������� ��������� ������.
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



// --------------------- �������������� ---------------------

FReply UMessageBoxPopup::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // �������� ����, ���� ���
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = true;

        // ���� ������� ������� (����� / ���������� �������)
        FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();

        // DragOffset = (��� ������) - (��� ������ ��������� ������� ����� ����)
        DragOffset = MousePos - CurrentPos;

        // ����������� ����, ����� ���������� �������� MouseMove, ���� ���� ������ ������ �� ������� �������
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
    // ���� �� � ������ ��������������
    if (bDragging)
    {
        FVector2D MousePos = InMouseEvent.GetScreenSpacePosition();
        FVector2D NewPos = MousePos - DragOffset; // ������� ���, ����� ������ ������� �� ��� �� ����� �������

        // �������� ������ ����
        FVector2D ViewportSize(0.f, 0.f);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(ViewportSize);
        }

        // ����� ������� ������� (������ "����������" �������, �� �� ���������� bRemoveDPIScale = true)
        FVector2D WidgetSize = GetDesiredSize();

        // ������������, ����� �� �������� �� ����
        float HalfW = WidgetSize.X * 0.5f;
        float HalfH = WidgetSize.Y * 0.5f;

        NewPos.X = FMath::Clamp(NewPos.X, HalfW, ViewportSize.X - HalfW);
        NewPos.Y = FMath::Clamp(NewPos.Y, HalfH, ViewportSize.Y - HalfH);

        // ��������� ����� �������
        CurrentPos = NewPos;

        // ������ ������ (true -> ���������� ��������� ����������� ���������)
        SetPositionInViewport(CurrentPos, true);

        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UMessageBoxPopup::NativeOnMouseButtonUp(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // ���� �� ������������� � ��������� ���, ����������� ����
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;

        // ����������� ������
        FEventReply EventReply = UWidgetBlueprintLibrary::Handled();
        EventReply = UWidgetBlueprintLibrary::ReleaseMouseCapture(EventReply);

        return EventReply.NativeReply;
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UMessageBoxPopup::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);

    // ���� �������� ������ (��������, �������� �� ��������� ����) � ���������� ����
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