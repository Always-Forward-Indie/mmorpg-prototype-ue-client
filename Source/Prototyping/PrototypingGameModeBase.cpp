// Copyright Epic Games, Inc. All Rights Reserved.


#include "PrototypingGameModeBase.h"
#include "Player/BasicPlayer.h"

APrototypingGameModeBase::APrototypingGameModeBase()
{
	// Disable auto-possession
	DefaultPawnClass = nullptr;
	//DefaultPawnClass = ABasicPlayer::StaticClass();
}
