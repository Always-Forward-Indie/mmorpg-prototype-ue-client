// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/TextBlock.h>
#include "MonitorStatsWidget.generated.h"


/**
 * 
 */
UCLASS()
class PROTOTYPING_API UMonitorStatsWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "PING")
	// set the login server ping value
	void SetLoginServerPingValue(const FString& Value);
	UFUNCTION(BlueprintCallable, Category = "PING")
	// set the game server ping value
	void SetGameServerPingValue(const FString& Value);

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* LoginServerPingValue;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* GameServerPingValue;


};
