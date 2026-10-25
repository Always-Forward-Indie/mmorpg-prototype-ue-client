// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UNetworkManager::ConnectGameServer()
{
	ConnectLink(EServerType::GameServer);
}
