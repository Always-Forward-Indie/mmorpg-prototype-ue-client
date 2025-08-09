// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/W_MOBHeadInfoWidget.h"


void UW_MOBHeadInfoWidget::SetWidgetScale(float Scale)
{

    FWidgetTransform Transform;
    Transform.Scale = FVector2D(Scale, Scale);
    SetRenderTransform(Transform);
}