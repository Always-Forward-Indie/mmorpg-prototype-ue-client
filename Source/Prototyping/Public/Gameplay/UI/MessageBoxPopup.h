// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/TextBlock.h>
#include <Components/Button.h>
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "MessageBoxPopup.generated.h"

// Делегаты для обработки нажатий кнопок
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMessageBoxDelegate);

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UMessageBoxPopup : public UUserWidget
{
	GENERATED_BODY()
	
    public:
    // Конструктор, где делаем виджет фокусируемым
    UMessageBoxPopup(const FObjectInitializer& ObjectInitializer);

    public:
        // Текст заголовка (должен быть привязан из дизайнера)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UTextBlock* TitleText;

        // Текст сообщения (должен быть привязан из дизайнера)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UTextBlock* MessageText;

        // Кнопка "Left" (привязана из дизайнера)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UButton* LeftButton;

        // Кнопка "Right" (привязана из дизайнера)
        UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "MessageBox")
        class UButton* RightButton;

        // Делегаты, которые будут вызываться при нажатии кнопок
        UPROPERTY(BlueprintAssignable, Category = "MessageBox")
        FMessageBoxDelegate OnLeftButtonClicked;

        UPROPERTY(BlueprintAssignable, Category = "MessageBox")
        FMessageBoxDelegate OnRightButtonClicked;

        // Функция для установки текста сообщения и надписей на кнопках
        UFUNCTION(BlueprintCallable, Category = "MessageBox")
        void SetupMessageBox(const FText& InTitle, const FText& InMessage, const FText& InLeftButtonLabel, const FText& InRightButtonLabel);


        // Переопределяем методы для обработки перетаскивания
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


        // Для перетаскивания
        bool bDragging = false;

        // Текущая позиция нашего виджета (в координатах экрана/вьюпорта)
        FVector2D CurrentPos = FVector2D::ZeroVector;

        // Смещение между курсором и верхним левым углом виджета при старте драга
        FVector2D DragOffset = FVector2D::ZeroVector;
};
