#include "Gameplay/Social/PlayerInspectManager.h"
#include "Prototyping.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UPlayerInspectManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
	if (!InNetworkManager || !InGameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("PlayerInspectManager: Initialize called with null parameters"));
		return;
	}
	NetworkManager = InNetworkManager;
	GameInstance = InGameInstance;
}

void UPlayerInspectManager::RequestInspect(int32 TargetCharacterId)
{
	if (!NetworkManager || !GameInstance || TargetCharacterId <= 0) return;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();

	Header->SetStringField(TEXT("eventType"), TEXT("playerInspectRequest"));
	Header->SetNumberField(TEXT("clientId"), GameInstance->GetCurrentClientID());
	Header->SetStringField(TEXT("hash"), GameInstance->GetCurrentClientHash());
	Body->SetNumberField(TEXT("targetCharacterId"), TargetCharacterId);

	Root->SetObjectField(TEXT("header"), Header);
	Root->SetObjectField(TEXT("body"), Body);

	FString Payload;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	SendPacket(Payload);
}

void UPlayerInspectManager::OnInspectReceived(const FPlayerInspectData& Data)
{
	OnInspectReceivedDelegate.Broadcast(Data);
}

void UPlayerInspectManager::OnInspectError(const FString& Reason)
{
	UE_LOG(LogNetPacket, Error, TEXT("PlayerInspectManager: inspect failed reason='%s'"), *Reason);
	OnInspectErrorDelegate.Broadcast(Reason);
}

void UPlayerInspectManager::SendPacket(const FString& JsonPayload)
{
	if (NetworkManager)
		NetworkManager->SendDataToChunkServer(JsonPayload);
}
