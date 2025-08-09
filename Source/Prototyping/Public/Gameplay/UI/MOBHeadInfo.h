// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include <Components/ProgressBar.h>
#include <Components/TextBlock.h>
#include "MOBHeadInfo.generated.h"

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UMOBHeadInfo : public UWidgetComponent
{
	GENERATED_BODY()
	
	protected:
		// Указатель на UI виджет
		UPROPERTY()
		UUserWidget* MobWidget;

		// Указатели на UI элементы
		UPROPERTY()
		UProgressBar* HealthBar;

		UPROPERTY()
		UProgressBar* ManaBar;

		UPROPERTY()
		UTextBlock* MobNameText;

		UPROPERTY()
		UTextBlock* MobLevelText;

		UPROPERTY()
		bool bIsAggressive = false;

	public:
		// Инициализация компонента
		virtual void BeginPlay() override;

		// Обновление информации (HP, MP, Имя, Уровень)
		void UpdateInfo(float CurrentHP, float MaxHP, float CurrentMP, float MaxMP, const FString& MobName, int MobLevel, bool isMobAggressive);

		// Обновление информации HP
		void UpdateHealth(float CurrentHP, float MaxHP);

		// Обновление информации MP
		void UpdateMana(float CurrentMP, float MaxMP);

		// Обновление информации Имя
		void UpdateMobName(const FString& MobName);

		// Обновление информации Уровень
		void UpdateMobLevel(int MobLevel);

		// Обновление информации Агрессивность
		void UpdateMobAggressive(bool isMobAggressive);

		// Показать / скрыть UI
		void ShowWidget(bool bShow);
};
