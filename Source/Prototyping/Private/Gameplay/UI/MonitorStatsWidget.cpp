// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/MonitorStatsWidget.h"

// set the login server ping value
void UMonitorStatsWidget::SetLoginServerPingValue(const FString& Value)
{
	LoginServerPingValue->SetText(FText::FromString(Value));
}

// set the game server ping value
void UMonitorStatsWidget::SetGameServerPingValue(const FString& Value)
{
	GameServerPingValue->SetText(FText::FromString(Value));
}