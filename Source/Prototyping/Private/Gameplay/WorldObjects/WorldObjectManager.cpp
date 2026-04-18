#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Engine/DataTable.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UWorldObjectManager::UWorldObjectManager()
{
}

void UWorldObjectManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
	if (!InNetworkManager || !InGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldObjectManager: Initialize called with null parameters"));
		return;
	}

	NetworkManager = InNetworkManager;
	GameInstance   = InGameInstance;
	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: Initialized"));
}

void UWorldObjectManager::SetWorldContext(UWorld* InWorld)
{
	WorldContext = InWorld;
}

// ─────────────────────────────────────────────────────────────────────────────
// Actor lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UWorldObjectManager::SpawnWorldObjects(const TArray<FWorldObjectData>& Objects)
{
	ClearWorldState();

	for (const FWorldObjectData& ObjData : Objects)
	{
		SpawnSingleObject(ObjData);
	}

	OnWorldObjectsSpawned.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: Spawned %d world objects"), Objects.Num());
}

AWorldInteractiveObjectActor* UWorldObjectManager::SpawnSingleObject(const FWorldObjectData& Data)
{
	if (!WorldContext)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldObjectManager: Cannot spawn — no WorldContext"));
		return nullptr;
	}

	// Determine actor class: DataTable lookup by slug, then fallback to default
	TSubclassOf<AWorldInteractiveObjectActor> ActorClass = DefaultActorClass;

	if (WIODefinitionTable)
	{
		const FWIODefinitionRow* Row = WIODefinitionTable->FindRow<FWIODefinitionRow>(
			FName(*Data.Slug), TEXT("WIO Spawn"), false);
		if (Row && Row->ActorClass)
		{
			// The DataTable may reference a generic AActor subclass; only use it if
			// it is actually derived from AWorldInteractiveObjectActor
			if (Row->ActorClass->IsChildOf(AWorldInteractiveObjectActor::StaticClass()))
			{
				ActorClass = *reinterpret_cast<const TSubclassOf<AWorldInteractiveObjectActor>*>(&Row->ActorClass);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("WorldObjectManager: DataTable row '%s' has ActorClass not derived from AWorldInteractiveObjectActor — using default"),
					*Data.Slug);
			}
		}
	}

	if (!ActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldObjectManager: No actor class for slug '%s' and no DefaultActorClass set"), *Data.Slug);
		return nullptr;
	}

	// Server coords → UE world coordinates
	const FVector  Location(Data.PosX, Data.PosY, Data.PosZ);
	const FRotator Rotation(0.f, Data.RotZ, 0.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWorldInteractiveObjectActor* NewActor = WorldContext->SpawnActor<AWorldInteractiveObjectActor>(
		ActorClass, Location, Rotation, SpawnParams);

	if (!NewActor)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldObjectManager: Failed to spawn actor for WIO id=%d slug='%s'"), Data.ObjectId, *Data.Slug);
		return nullptr;
	}

	NewActor->InitializeFromServerData(Data);
	ObjectRegistry.Add(Data.ObjectId, NewActor);
	ObjectDataRegistry.Add(Data.ObjectId, Data);

	OnWorldObjectActorSpawned.Broadcast(NewActor);
	return NewActor;
}

void UWorldObjectManager::ClearWorldState()
{
	// Cancel any active channel
	ActiveChannelObjectId = 0;
	ChannelDuration       = 0.f;
	ChannelStartTime      = 0.0;

	for (auto& Pair : ObjectRegistry)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	ObjectRegistry.Empty();
	ObjectDataRegistry.Empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// Client → Server
// ─────────────────────────────────────────────────────────────────────────────

void UWorldObjectManager::RequestInteract(int32 ObjectId)
{
	if (!NetworkManager || !GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldObjectManager::RequestInteract: not initialized"));
		return;
	}

	// Cooldown guard
	const double Now = FPlatformTime::Seconds();
	if (Now - LastInteractRequestTime < InteractCooldownSec)
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldObjectManager::RequestInteract: cooldown active, ignoring"));
		return;
	}
	LastInteractRequestTime = Now;

	const int32 ClientId     = GameInstance->GetCurrentClientID();
	const int32 CharacterId  = GameInstance->GetCurrentCharacterID();
	const FString Hash       = GameInstance->GetCurrentClientHash();

	TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

	Header->SetStringField(TEXT("eventType"), TEXT("worldObjectInteract"));
	Header->SetNumberField(TEXT("clientId"),  ClientId);
	Header->SetStringField(TEXT("hash"),      Hash);

	Body->SetNumberField(TEXT("characterId"), CharacterId);
	Body->SetNumberField(TEXT("objectId"),    ObjectId);

	Root->SetObjectField(TEXT("header"), Header);
	Root->SetObjectField(TEXT("body"),   Body);

	FString Payload;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	NetworkManager->SendDataToChunkServer(Payload);

	OnInteractionStarted.Broadcast(ObjectId);
	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: Sent worldObjectInteract objectId=%d"), ObjectId);
}

void UWorldObjectManager::RequestCancelChannel(int32 ObjectId)
{
	if (!NetworkManager || !GameInstance)
	{
		return;
	}

	if (ActiveChannelObjectId == 0)
	{
		return;
	}

	const int32 ClientId     = GameInstance->GetCurrentClientID();
	const int32 CharacterId  = GameInstance->GetCurrentCharacterID();
	const FString Hash       = GameInstance->GetCurrentClientHash();

	TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

	Header->SetStringField(TEXT("eventType"), TEXT("worldObjectChannelCancel"));
	Header->SetNumberField(TEXT("clientId"),  ClientId);
	Header->SetStringField(TEXT("hash"),      Hash);

	Body->SetNumberField(TEXT("characterId"), CharacterId);
	Body->SetNumberField(TEXT("objectId"),    ObjectId);

	Root->SetObjectField(TEXT("header"), Header);
	Root->SetObjectField(TEXT("body"),   Body);

	FString Payload;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	NetworkManager->SendDataToChunkServer(Payload);

	// Immediately clear local channel state
	ActiveChannelObjectId = 0;
	ChannelDuration       = 0.f;
	ChannelStartTime      = 0.0;

	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: Sent worldObjectChannelCancel objectId=%d"), ObjectId);
}

void UWorldObjectManager::CancelActiveChannel()
{
	if (ActiveChannelObjectId != 0)
	{
		RequestCancelChannel(ActiveChannelObjectId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Server → Client (called by WIONetworkHandler)
// ─────────────────────────────────────────────────────────────────────────────

void UWorldObjectManager::HandleSpawnWorldObjects(const TArray<FWorldObjectData>& Objects)
{
	SpawnWorldObjects(Objects);
}

void UWorldObjectManager::HandleInteractResult(const FWIOInteractResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: InteractResult objectId=%d success=%d type=%s"),
		Result.ObjectId, Result.bSuccess, *Result.InteractionType);

	if (Result.bSuccess)
	{
		// Start channeling if this is a channeled interaction with a duration
		if (Result.InteractionType == TEXT("channeled") && Result.ChannelTimeSec > 0)
		{
			ActiveChannelObjectId = Result.ObjectId;
			ChannelDuration       = static_cast<float>(Result.ChannelTimeSec);
			ChannelStartTime      = FPlatformTime::Seconds();
		}

		// Channeled complete — channel finished on server
		if (Result.InteractionType == TEXT("channeled_complete"))
		{
			ActiveChannelObjectId = 0;
			ChannelDuration       = 0.f;
			ChannelStartTime      = 0.0;
		}

		// Update actor visual state for per-player objects that deplete on use
		AWorldInteractiveObjectActor* Actor = GetObjectActorById(Result.ObjectId);
		if (Actor)
		{
			Actor->OnInteractResultReceived(Result);
		}
	}

	OnInteractResult.Broadcast(Result);
}

void UWorldObjectManager::HandleStateUpdate(const FWIOStateUpdate& Update)
{
	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: StateUpdate objectId=%d state=%d respawnSec=%d"),
		Update.ObjectId, static_cast<int32>(Update.NewState), Update.RespawnSec);

	AWorldInteractiveObjectActor* Actor = GetObjectActorById(Update.ObjectId);
	if (Actor)
	{
		Actor->SetObjectState(Update.NewState, Update.RespawnSec);
	}

	OnStateChanged.Broadcast(Update);
}

void UWorldObjectManager::HandleChannelCancelled(int32 ObjectId)
{
	UE_LOG(LogTemp, Log, TEXT("WorldObjectManager: ChannelCancelled objectId=%d"), ObjectId);

	if (ActiveChannelObjectId == ObjectId)
	{
		ActiveChannelObjectId = 0;
		ChannelDuration       = 0.f;
		ChannelStartTime      = 0.0;
	}

	OnChannelCancelled.Broadcast(ObjectId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

AWorldInteractiveObjectActor* UWorldObjectManager::GetObjectActorById(int32 ObjectId) const
{
	const TWeakObjectPtr<AWorldInteractiveObjectActor>* Found = ObjectRegistry.Find(ObjectId);
	if (Found && Found->IsValid())
	{
		return Found->Get();
	}
	return nullptr;
}

TArray<AWorldInteractiveObjectActor*> UWorldObjectManager::GetAllObjectActors() const
{
	TArray<AWorldInteractiveObjectActor*> Result;
	for (const auto& Pair : ObjectRegistry)
	{
		if (Pair.Value.IsValid())
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}

bool UWorldObjectManager::IsObjectRegistered(int32 ObjectId) const
{
	const TWeakObjectPtr<AWorldInteractiveObjectActor>* Found = ObjectRegistry.Find(ObjectId);
	return Found && Found->IsValid();
}

float UWorldObjectManager::GetChannelProgress() const
{
	if (ActiveChannelObjectId == 0 || ChannelDuration <= 0.f)
	{
		return 0.f;
	}

	const double Elapsed = FPlatformTime::Seconds() - ChannelStartTime;
	return FMath::Clamp(static_cast<float>(Elapsed / ChannelDuration), 0.f, 1.f);
}
