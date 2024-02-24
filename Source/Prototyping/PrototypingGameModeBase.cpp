// Copyright Epic Games, Inc. All Rights Reserved.


#include "PrototypingGameModeBase.h"
#include "Gameplay/Players/BasicPlayer.h"

APrototypingGameModeBase::APrototypingGameModeBase()
{
	// Disable auto-possession
	DefaultPawnClass = nullptr;
	//DefaultPawnClass = ABasicPlayer::StaticClass();
}
