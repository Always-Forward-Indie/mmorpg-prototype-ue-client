#include "Gameplay/NPCs/AmbientSpeechNetworkHandler.h"
#include "Prototyping.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "MyGameInstance.h"
#include "EngineUtils.h"
#include "Services/TimeSyncService.h"
#include "Networking/NetworkManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UAmbientSpeechNetworkHandler::Initialize(UAmbientSpeechManager* InManager, UNetworkManager* InNetworkManager)
{
    if (!InManager || !InNetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("AmbientSpeechNetworkHandler: Initialize called with null parameters"));
        return;
    }
    AmbientSpeechManager = InManager;
    NetworkManager = InNetworkManager;
}

void UAmbientSpeechNetworkHandler::SubscribeToNetworkEvents()
{
    if (bIsSubscribed || !NetworkManager) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    for (const FString& EventType : HandledEvents())
    {
        Dispatcher.AddHandler(EventType, this, &UAmbientSpeechNetworkHandler::HandleChunkPacket);
    }
    bIsSubscribed = true;
}

void UAmbientSpeechNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!bIsSubscribed || !NetworkManager) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    for (const FString& EventType : HandledEvents())
    {
        Dispatcher.RemoveHandler(EventType, this);
    }
    bIsSubscribed = false;
}

void UAmbientSpeechNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
    // Single-parse path (S05): EventType comes pre-parsed from the dispatcher.
    if (Packet.EventType == TEXT("NPC_AMBIENT_POOLS"))
    {
        // Compat alias: HandleAmbientPools below still takes the raw string (S05b converts it).
        const FString& ReceivedData = Packet.Raw;
        HandleAmbientPools(ReceivedData);
    }
}

// EventTypes owned by this handler (single list drives dispatcher registration).
const TArray<FString>& UAmbientSpeechNetworkHandler::HandledEvents()
{
    static const TArray<FString> Events = { TEXT("NPC_AMBIENT_POOLS") };
    return Events;
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

    UE_LOG(LogGameplay, Verbose, TEXT("AmbientSpeechNetworkHandler: Stored pools for %d NPC(s)"), NpcsArr->Num());

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
                        UE_LOG(LogGameplay, Verbose, TEXT("AmbientSpeechNetworkHandler: Backfilled ambient data for NPC %d"), NPC->GetNPCId());
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
