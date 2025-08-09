// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScaleBox.h"
#include "W_MOBHeadInfoWidget.generated.h"

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UW_MOBHeadInfoWidget : public UUserWidget
{
	GENERATED_BODY()
	

public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetScale(float Scale);

	UPROPERTY(meta = (BindWidget))
	class UScaleBox* RootScaleBox;
};
