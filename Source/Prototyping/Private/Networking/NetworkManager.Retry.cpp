// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonUtilities.h"
#include <Kismet/KismetSystemLibrary.h>
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/RetryPolicy.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UNetworkManager::OnLoginServerConnectionRetry()
{
	if (MsgBoxLoginServer)
	{
		MsgBoxLoginServer->OnLeftButtonClicked.Clear();
		MsgBoxLoginServer->OnRightButtonClicked.Clear();
		MsgBoxLoginServer->RemoveFromParent();
		MsgBoxLoginServer = nullptr;
	}

	UE_LOG(LogConnection, Log, TEXT("User chose to retry Login Server connection."));
	// ������� ���������������: �������� ����� ����������� ������
	ConnectLoginServer();
}

void UNetworkManager::OnGameServerConnectionRetry()
{
	UE_LOG(LogConnection, Log, TEXT("User chose to retry Game Server connection."));
	if (MsgBoxGameServer) 
	{
		MsgBoxGameServer->OnLeftButtonClicked.Clear();
		MsgBoxGameServer->OnRightButtonClicked.Clear();
		MsgBoxGameServer->RemoveFromParent();
		MsgBoxGameServer = nullptr;
	}

	// ������� ���������������: �������� ����� ����������� ������
	ConnectGameServer();
}

// retry connection to chunk server
void UNetworkManager::OnChunkServerConnectionRetry()
{
	if (MsgBoxChunkServer)
	{
		MsgBoxChunkServer->OnLeftButtonClicked.Clear();
		MsgBoxChunkServer->OnRightButtonClicked.Clear();
		MsgBoxChunkServer->RemoveFromParent();
		MsgBoxChunkServer = nullptr;
	}
	UE_LOG(LogConnection, Log, TEXT("User chose to retry Chunk Server connection."));
	// ������� ���������������: �������� ����� ����������� ������
	ConnectChunkServer();
}


// if cancel connection
void UNetworkManager::OnConnectCancel()
{
	UE_LOG(LogConnection, Log, TEXT("User cancelled Server connection."));
	// ��������� ���� ��� ��������� ������ ������ ������
	UKismetSystemLibrary::QuitGame(WorldContext, nullptr, EQuitPreference::Quit, true);
}


