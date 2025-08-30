// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/MonitorStatsWidget.h"

// set the login server ping value
void UMonitorStatsWidget::SetLoginServerPingValue(const FString& Value)
{
	if (LoginServerPingValue)
	{
		LoginServerPingValue->SetText(FText::FromString(Value));
	}
}

// set the game server ping value
void UMonitorStatsWidget::SetGameServerPingValue(const FString& Value)
{
	if (GameServerPingValue)
	{
		GameServerPingValue->SetText(FText::FromString(Value));
	}
}

// set the chunk server ping value
void UMonitorStatsWidget::SetChunkServerPingValue(const FString& Value)
{
	if (ChunkServerPingValue)
	{
		ChunkServerPingValue->SetText(FText::FromString(Value));
	}
}