#include "Gameplay/NPCs/AmbientSpeechNetworkHandler.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "MyGameInstance.h"
#include "EngineUtils.h"
#include "Networking/NetworkManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UAmbientSpeechNetworkHandler::Initialize(UAmbientSpeechManager* InManager, UNetworkManager* InNetworkManager)
{
    if (!InManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("AmbientSpeechNetworkHandler: Initialize called with null parameters"));
        return;
    }
    AmbientSpeechManager = InManager;
    NetworkManager = InNetworkManager;
}

void UAmbientSpeechNetworkHandler::SubscribeToNetworkEvents()
{
    if (bIsSubscribed || !NetworkManager) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UAmbientSpeechNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UAmbientSpeechNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!bIsSubscribed || !NetworkManager) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UAmbientSpeechNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UAmbientSpeechNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* HeaderObj = nullptr;
    if (!Root->TryGetObjectField(TEXT("header"), HeaderObj)) return;

    FString EventType;
    if (!(*HeaderObj)->TryGetStringField(TEXT("eventType"), EventType)) return;

    if (EventType == TEXT("NPC_AMBIENT_POOLS"))
    {
        HandleAmbientPools(ReceivedData);
    }
}

void UAmbientSpeechNetworkHandler::HandleAmbientPools(const FString& JsonData)
{
    if (!AmbientSpeechManager) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyObj = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyObj)) return;

    const TArray<TSharedPtr<FJsonValue>>* NpcsArr = nullptr;
    if (!(*BodyObj)->TryGetArrayField(TEXT("npcs"), NpcsArr)) return;

    for (const TSharedPtr<FJsonValue>& NpcVal : *NpcsArr)
    {
        const TSharedPtr<FJsonObject> NpcObj = NpcVal->AsObject();
        if (!NpcObj.IsValid()) continue;

        FAmbientSpeechNPCData NpcData;
        NpcData.npcId = NpcObj->GetIntegerField(TEXT("npcId"));
        NpcData.minIntervalSec = NpcObj->GetNumberField(TEXT("minIntervalSec"));
        NpcData.maxIntervalSec = NpcObj->GetNumberField(TEXT("maxIntervalSec"));

        const TArray<TSharedPtr<FJsonValue>>* PoolsArr = nullptr;
        if (NpcObj->TryGetArrayField(TEXT("pools"), PoolsArr))
        {
            for (const TSharedPtr<FJsonValue>& PoolVal : *PoolsArr)
            {
                const TSharedPtr<FJsonObject> PoolObj = PoolVal->AsObject();
                if (PoolObj.IsValid())
                {
                    NpcData.pools.Add(ParsePool(PoolObj));
                }
            }
        }

        AmbientSpeechManager->SetAmbientSpeechPools(NpcData.npcId, NpcData);
    }

    UE_LOG(LogTemp, Log, TEXT("AmbientSpeechNetworkHandler: Stored pools for %d NPC(s)"), NpcsArr->Num());

    // Backfill any BasicNPC actors that were already spawned before this event arrived
    if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(AmbientSpeechManager->GetOuter()))
    {
        if (UWorld* World = MyGI->GetWorld())
        {
            for (TActorIterator<ABasicNPC> It(World); It; ++It)
            {
                ABasicNPC* NPC = *It;
                FAmbientSpeechNPCData OutData;
                if (AmbientSpeechManager->GetNPCAmbientData(NPC->GetNPCId(), OutData))
                {
                    if (UNPCAmbientSpeechComponent* ASComp = NPC->FindComponentByClass<UNPCAmbientSpeechComponent>())
                    {
                        ASComp->SetAmbientData(OutData);
                        UE_LOG(LogTemp, Log, TEXT("AmbientSpeechNetworkHandler: Backfilled ambient data for NPC %d"), NPC->GetNPCId());
                    }
                }
            }
        }
    }
}

FAmbientSpeechPoolData UAmbientSpeechNetworkHandler::ParsePool(const TSharedPtr<FJsonObject>& Obj) const
{
    FAmbientSpeechPoolData Pool;
    Pool.priority = Obj->GetIntegerField(TEXT("priority"));

    const TArray<TSharedPtr<FJsonValue>>* LinesArr = nullptr;
    if (Obj->TryGetArrayField(TEXT("lines"), LinesArr))
    {
        for (const TSharedPtr<FJsonValue>& LineVal : *LinesArr)
        {
            const TSharedPtr<FJsonObject> LineObj = LineVal->AsObject();
            if (LineObj.IsValid())
            {
                Pool.lines.Add(ParseLine(LineObj));
            }
        }
    }
    return Pool;
}

FAmbientSpeechLineData UAmbientSpeechNetworkHandler::ParseLine(const TSharedPtr<FJsonObject>& Obj) const
{
    FAmbientSpeechLineData Line;
    Line.id          = Obj->GetIntegerField(TEXT("id"));
    Obj->TryGetStringField(TEXT("lineKey"),     Line.lineKey);
    Obj->TryGetStringField(TEXT("triggerType"), Line.triggerType);
    Line.triggerRadius = (float)Obj->GetNumberField(TEXT("triggerRadius"));
    Line.weight        = Obj->HasField(TEXT("weight"))      ? (int32)Obj->GetNumberField(TEXT("weight"))      : 10;
    Line.cooldownSec   = Obj->HasField(TEXT("cooldownSec")) ? (float)Obj->GetNumberField(TEXT("cooldownSec")) : 60.f;
    return Line;
}
