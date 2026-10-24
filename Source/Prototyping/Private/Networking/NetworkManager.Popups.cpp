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

// S5: connection-issue popups gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "NetworkPopups"

UTimeSyncService* UNetworkManager::GetTimeSyncService()
{
    if (WorldContext)
    {
        UMyGameInstance* GameInstance = Cast<UMyGameInstance>(WorldContext->GetGameInstance());
        if (GameInstance)
        {
            return GameInstance->GetTimeSyncService();
        }
    }
    return nullptr;
}

// show popup for chunk server connection issue
void UNetworkManager::ShowChunkServerConnectionIssuePopup()
{
	if (!WorldContext || bIsShutDown) return;

	UClass* PopupClass = MessageBoxPopupClass.LoadSynchronous();
	if (!IsValid(PopupClass)) return;

	// ���� ���� ��� �������, �� ������ �����
	if (!MsgBoxChunkServer)
	{
		MsgBoxChunkServer = CreateWidget<UMessageBoxPopup>(WorldContext, PopupClass);
	}
	if (MsgBoxChunkServer)
	{
		FText TitleMessage = LOCTEXT("Error", "Error");
		FText ErrorMessage = LOCTEXT("ChunkRetry", "Can not connect to Chunk Server. Retry?");
		FText YesText = LOCTEXT("Yes", "Yes");
		FText NoText = LOCTEXT("No", "No");
		MsgBoxChunkServer->SetupMessageBox(TitleMessage, ErrorMessage, YesText, NoText);
		// ������������� ���� �� �����
		MsgBoxChunkServer->AddToViewport(100);
		// ������������� �� ������� ������
		MsgBoxChunkServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnChunkServerConnectionRetry);
		MsgBoxChunkServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
	}
}

// show popup for login server connection issue
void UNetworkManager::ShowLoginServerConnectionIssuePopup()
{
	if (!WorldContext || bIsShutDown) return;

	UClass* PopupClass = MessageBoxPopupClass.LoadSynchronous();
	if (!IsValid(PopupClass)) return;

	// ���� ���� ��� �������, �� ������ �����
	if (!MsgBoxLoginServer)
	{
		MsgBoxLoginServer = CreateWidget<UMessageBoxPopup>(WorldContext, PopupClass);
	}
	if (MsgBoxLoginServer)
	{

		FText TitleMessage = LOCTEXT("Error", "Error");
		FText ErrorMessage = LOCTEXT("LoginRetry", "Can not connect to Login Server. Retry?");
		FText YesText = LOCTEXT("Yes", "Yes");
		FText NoText = LOCTEXT("No", "No");
		MsgBoxLoginServer->SetupMessageBox(TitleMessage, ErrorMessage, YesText, NoText);

		// ������������� ���� �� �����
		MsgBoxLoginServer->AddToViewport(100);

		// ������������� �� ������� ������
		MsgBoxLoginServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnLoginServerConnectionRetry);
		MsgBoxLoginServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
	}
}

void UNetworkManager::ShowGameServerConnectionIssuePopup()
{
	if (!WorldContext || bIsShutDown) return;

	UClass* PopupClass = MessageBoxPopupClass.LoadSynchronous();
	if (!IsValid(PopupClass)) return;

	// ���� ���� ��� �������, �� ������ �����
	if (!MsgBoxGameServer)
	{
		MsgBoxGameServer = CreateWidget<UMessageBoxPopup>(WorldContext, PopupClass);
	}
	if (MsgBoxGameServer)
	{
		FText TitleMessage = LOCTEXT("Error", "Error");
		FText ErrorMessage = LOCTEXT("GameRetry", "Can not connect to Game Server. Retry?");
		FText YesText = LOCTEXT("Yes", "Yes");
		FText NoText = LOCTEXT("No", "No");
		MsgBoxGameServer->SetupMessageBox(TitleMessage, ErrorMessage, YesText, NoText);

		// ������������� ���� �� �����
		MsgBoxGameServer->AddToViewport(100);

		// ������������� �� ������� ������
		MsgBoxGameServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnGameServerConnectionRetry);
		MsgBoxGameServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
	}
}

#undef LOCTEXT_NAMESPACE


