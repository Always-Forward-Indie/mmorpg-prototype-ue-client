#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "MyGameInstance.h"

void UAmbientSpeechManager::Initialize(UMyGameInstance* InGameInstance)
{
    GameInstance = InGameInstance;
}

void UAmbientSpeechManager::SetAmbientSpeechPools(int32 NpcId, const FAmbientSpeechNPCData& Data)
{
    AmbientData.Add(NpcId, Data);
}

bool UAmbientSpeechManager::GetNPCAmbientData(int32 NpcId, FAmbientSpeechNPCData& OutData) const
{
    const FAmbientSpeechNPCData* Found = AmbientData.Find(NpcId);
    if (Found)
    {
        OutData = *Found;
        return true;
    }
    return false;
}

bool UAmbientSpeechManager::HasNPCAmbientData(int32 NpcId) const
{
    return AmbientData.Contains(NpcId);
}

void UAmbientSpeechManager::ClearAll()
{
    AmbientData.Empty();
}
