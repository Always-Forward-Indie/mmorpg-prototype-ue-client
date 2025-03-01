// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/TextBlock.h>
#include <Components/Button.h>
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "MessageBoxPopup.generated.h"

// �������� ��� ��������� ������� ������
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMessageBoxDelegate);

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UMessageBoxPopup : public UUserWidget
{
	GENERATED_BODY()
	
    public:
    // �����������, ��� ������ ������ ������������
    UMessageBoxPopup(const FObjectInitializer& ObjectInitializer);

    public:
        // ����� ��������� (������ ���� �������� �� ���������)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UTextBlock* TitleText;

        // ����� ��������� (������ ���� �������� �� ���������)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UTextBlock* MessageText;

        // ������ "Left" (��������� �� ���������)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UButton* LeftButton;

        // ������ "Right" (��������� �� ���������)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UButton* RightButton;

        // ��������, ������� ����� ���������� ��� ������� ������
        UPROPERTY(BlueprintAssignable, Category = "MessageBox")
        FMessageBoxDelegate OnLeftButtonClicked;

        UPROPERTY(BlueprintAssignable, Category = "MessageBox")
        FMessageBoxDelegate OnRightButtonClicked;

        // ������� ��� ��������� ������ ��������� � �������� �� �������
        UFUNCTION(BlueprintCallable, Category = "MessageBox")
        void SetupMessageBox(const FText& InTitle, const FText& InMessage, const FText& InLeftButtonLabel, const FText& InRightButtonLabel);


        // �������������� ������ ��� ��������� ��������������
        virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
        virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
        void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent);
        virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

        virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;


    protected:
        virtual void NativeConstruct() override;

    private:
        UFUNCTION()
        void HandleLeftButtonClicked();

        UFUNCTION()
        void HandleRightButtonClicked();


        // ��� ��������������
        bool bDragging = false;

        // ������� ������� ������ ������� (� ����������� ������/��������)
        FVector2D CurrentPos = FVector2D::ZeroVector;

        // �������� ����� �������� � ������� ����� ����� ������� ��� ������ �����
        FVector2D DragOffset = FVector2D::ZeroVector;
};
