#include "DevMode/DevModeDataProvider.h"
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ============================================================================
void UDevModeDataProvider::Initialize(UMyGameInstance* InGameInstance, const FDevModeConfig& InConfig)
{
    GameInstance = InGameInstance;
    Config       = InConfig;
}

// ============================================================================
FString UDevModeDataProvider::ResolveFilePath(const FString& RelativePath) const
{
    // Try project root first (works for Config/DevMode/*)
    const FString ProjectRoot = FPaths::ProjectDir();
    const FString Absolute    = FPaths::Combine(ProjectRoot, RelativePath);
    return FPaths::ConvertRelativePathToFull(Absolute);
}

FString UDevModeDataProvider::ReadJsonFile(const FString& RelativePath) const
{
    const FString AbsPath = ResolveFilePath(RelativePath);
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *AbsPath))
    {
        UE_LOG(LogTemp, Error, TEXT("DevModeDataProvider: Failed to read '%s'"), *AbsPath);
        return FString();
    }
    return JsonStr;
}

// ============================================================================
// Player data
// ============================================================================
bool UDevModeDataProvider::LoadPlayerData(FClientDataStruct& OutClientData) const
{
    const FString JsonStr = ReadJsonFile(Config.PlayerDataJsonPath);
    if (JsonStr.IsEmpty()) return false;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("DevModeDataProvider: Failed to parse player JSON"));
        return false;
    }

    OutClientData.clientId    = Root->GetIntegerField(TEXT("clientId"));
    OutClientData.clientLogin = Root->GetStringField(TEXT("clientLogin"));
    OutClientData.hash        = Root->GetStringField(TEXT("hash"));

    const TSharedPtr<FJsonObject>* CharObj = nullptr;
    if (!Root->TryGetObjectField(TEXT("characterData"), CharObj) || !CharObj)
    {
        UE_LOG(LogTemp, Error, TEXT("DevModeDataProvider: Missing 'characterData' in player JSON"));
        return false;
    }

    FCharacterDataStruct& CD = OutClientData.characterData;
    CD.characterId              = (*CharObj)->GetIntegerField(TEXT("characterId"));
    CD.characterName            = (*CharObj)->GetStringField(TEXT("characterName"));
    CD.characterClass           = (*CharObj)->GetStringField(TEXT("characterClass"));
    CD.characterRace            = (*CharObj)->GetStringField(TEXT("characterRace"));
    CD.characterLevel           = (*CharObj)->GetIntegerField(TEXT("characterLevel"));
    CD.characterCurrentHealth   = (*CharObj)->GetIntegerField(TEXT("characterCurrentHealth"));
    CD.characterCurrentMana     = (*CharObj)->GetIntegerField(TEXT("characterCurrentMana"));
    CD.characterExperiencePoints = (*CharObj)->GetIntegerField(TEXT("characterExperiencePoints"));
    CD.characterExpForLevelStart = (*CharObj)->GetIntegerField(TEXT("characterExpForLevelStart"));
    CD.characterExpForLevelEnd   = (*CharObj)->GetIntegerField(TEXT("characterExpForLevelEnd"));
    CD.characterExperienceDebt   = (*CharObj)->GetIntegerField(TEXT("characterExperienceDebt"));

    const TSharedPtr<FJsonObject>* PosObj = nullptr;
    if ((*CharObj)->TryGetObjectField(TEXT("characterPosition"), PosObj) && PosObj)
    {
        CD.characterPosition.positionX = (*PosObj)->GetNumberField(TEXT("positionX"));
        CD.characterPosition.positionY = (*PosObj)->GetNumberField(TEXT("positionY"));
        CD.characterPosition.positionZ = (*PosObj)->GetNumberField(TEXT("positionZ"));
        CD.characterPosition.rotationZ = (*PosObj)->GetNumberField(TEXT("rotationZ"));
    }

    const TSharedPtr<FJsonObject>* AttrsObj = nullptr;
    if ((*CharObj)->TryGetObjectField(TEXT("characterAttributes"), AttrsObj) && AttrsObj)
    {
        for (auto& Pair : (*AttrsObj)->Values)
        {
            const TSharedPtr<FJsonObject>* AttrEntry = nullptr;
            if (Pair.Value->TryGetObject(AttrEntry) && AttrEntry)
            {
                FAttributeDataStruct Attr;
                Attr.attributeId    = (*AttrEntry)->GetIntegerField(TEXT("attributeId"));
                Attr.attributeSlug  = (*AttrEntry)->GetStringField(TEXT("attributeSlug"));
                Attr.attributeName  = (*AttrEntry)->GetStringField(TEXT("attributeName"));
                Attr.attributeValue = (*AttrEntry)->GetIntegerField(TEXT("attributeValue"));
                CD.characterAttributes.attributesData.Add(Pair.Key, Attr);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("DevModeDataProvider: Loaded player '%s' (id=%d)"),
        *CD.characterName, CD.characterId);
    return true;
}

// ============================================================================
// Mobs
// ============================================================================
bool UDevModeDataProvider::ParseMobEntry(const TSharedPtr<FJsonObject>& J, FMOBStruct& M) const
{
    if (!J.IsValid()) return false;

    M.mobID       = J->GetIntegerField(TEXT("mobID"));
    M.mobUniqueID = J->GetStringField(TEXT("mobUniqueID"));
    M.mobZoneID   = J->GetIntegerField(TEXT("mobZoneID"));
    M.mobName     = J->GetStringField(TEXT("mobName"));
    M.mobSlug     = J->GetStringField(TEXT("mobSlug"));
    M.mobRace     = J->GetStringField(TEXT("mobRace"));
    M.mobLevel    = J->GetIntegerField(TEXT("mobLevel"));
    M.mobCurrentHealth = J->GetIntegerField(TEXT("mobCurrentHealth"));
    M.mobCurrentMana   = J->GetIntegerField(TEXT("mobCurrentMana"));
    M.bIsDead          = J->GetBoolField(TEXT("bIsDead"));
    M.bIsAggressive    = J->GetBoolField(TEXT("bIsAggressive"));
    M.bIsMoving        = J->GetBoolField(TEXT("bIsMoving"));
    M.mobCombatState   = J->GetIntegerField(TEXT("mobCombatState"));
    M.mobTargetId      = J->GetIntegerField(TEXT("mobTargetId"));
    M.mobTargetType    = J->GetStringField(TEXT("mobTargetType"));

    const TSharedPtr<FJsonObject>* PosObj = nullptr;
    if (J->TryGetObjectField(TEXT("mobPosition"), PosObj) && PosObj)
    {
        M.mobPosition.positionX = (*PosObj)->GetNumberField(TEXT("positionX"));
        M.mobPosition.positionY = (*PosObj)->GetNumberField(TEXT("positionY"));
        M.mobPosition.positionZ = (*PosObj)->GetNumberField(TEXT("positionZ"));
        M.mobPosition.rotationZ = (*PosObj)->GetNumberField(TEXT("rotationZ"));
    }

    const TSharedPtr<FJsonObject>* VelObj = nullptr;
    if (J->TryGetObjectField(TEXT("mobVelocity"), VelObj) && VelObj)
    {
        M.mobVelocity.dirX  = static_cast<float>((*VelObj)->GetNumberField(TEXT("dirX")));
        M.mobVelocity.dirY  = static_cast<float>((*VelObj)->GetNumberField(TEXT("dirY")));
        M.mobVelocity.speed = static_cast<float>((*VelObj)->GetNumberField(TEXT("speed")));
    }

    const TSharedPtr<FJsonObject>* AttrsObj = nullptr;
    if (J->TryGetObjectField(TEXT("mobAttributes"), AttrsObj) && AttrsObj)
    {
        for (auto& Pair : (*AttrsObj)->Values)
        {
            const TSharedPtr<FJsonObject>* AE = nullptr;
            if (Pair.Value->TryGetObject(AE) && AE)
            {
                FAttributeDataStruct Attr;
                Attr.attributeId    = (*AE)->GetIntegerField(TEXT("attributeId"));
                Attr.attributeSlug  = (*AE)->GetStringField(TEXT("attributeSlug"));
                Attr.attributeName  = (*AE)->GetStringField(TEXT("attributeName"));
                Attr.attributeValue = (*AE)->GetIntegerField(TEXT("attributeValue"));
                M.mobAttributes.attributesData.Add(Pair.Key, Attr);
            }
        }
    }
    return true;
}

void UDevModeDataProvider::PopulateMobs(UMOBManager* InMobManager) const
{
    if (!InMobManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("DevModeDataProvider: PopulateMobs called with null MOBManager"));
        return;
    }

    const FString JsonStr = ReadJsonFile(Config.MobDataJsonPath);
    if (JsonStr.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TArray<TSharedPtr<FJsonValue>>* MobsArr = nullptr;
    if (!Root->TryGetArrayField(TEXT("mobs"), MobsArr) || !MobsArr) return;

    int32 Spawned = 0;
    for (const TSharedPtr<FJsonValue>& Entry : *MobsArr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Entry->TryGetObject(ObjPtr) || !ObjPtr) continue;

        FMOBStruct MobData;
        if (ParseMobEntry(*ObjPtr, MobData))
        {
            InMobManager->SpawnMOB(MobData);
            ++Spawned;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("DevModeDataProvider: Spawned %d test mobs"), Spawned);
}

// ============================================================================
// Inventory
// ============================================================================
bool UDevModeDataProvider::ParseInventoryItem(const TSharedPtr<FJsonObject>& J,
                                               FInventoryItemStruct& Item,
                                               int32 CharacterId) const
{
    if (!J.IsValid()) return false;

    Item.characterId  = CharacterId;
    Item.id           = J->GetIntegerField(TEXT("itemId"));
    Item.itemId       = Item.id;
    Item.slug         = J->GetStringField(TEXT("itemSlug"));
    Item.quantity     = J->GetIntegerField(TEXT("itemCount"));
    Item.weight       = static_cast<float>(J->GetNumberField(TEXT("itemWeight")));
    Item.priceBuy     = J->GetIntegerField(TEXT("itemPrice"));

    const FString TypeStr = J->GetStringField(TEXT("itemType"));
    Item.itemTypeSlug   = TypeStr.ToLower();

    if (TypeStr.Equals(TEXT("Weapon"), ESearchCase::IgnoreCase))
    {
        Item.item_type_id = 1;
        Item.isEquippable = true;
    }
    else if (TypeStr.Equals(TEXT("Armor"), ESearchCase::IgnoreCase))
    {
        Item.item_type_id = 2;
        Item.isEquippable = true;
    }
    else if (TypeStr.Equals(TEXT("Consumable"), ESearchCase::IgnoreCase))
    {
        Item.item_type_id = 3;
        Item.isUsable     = true;
    }
    else
    {
        Item.item_type_id = 0;
    }

    Item.rarityId   = 1;
    Item.raritySlug = TEXT("common");
    Item.stackSize  = Item.quantity;

    return true;
}

void UDevModeDataProvider::PopulateInventory(UInventoryManager* InInventoryManager,
                                             int32 CharacterId) const
{
    if (!InInventoryManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("DevModeDataProvider: PopulateInventory called with null InventoryManager"));
        return;
    }

    const FString JsonStr = ReadJsonFile(Config.InventoryDataJsonPath);
    if (JsonStr.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
    if (!Root->TryGetArrayField(TEXT("items"), ItemsArr) || !ItemsArr) return;

    int32 Added = 0;
    for (const TSharedPtr<FJsonValue>& Entry : *ItemsArr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Entry->TryGetObject(ObjPtr) || !ObjPtr) continue;

        FInventoryItemStruct Item;
        if (ParseInventoryItem(*ObjPtr, Item, CharacterId))
        {
            InInventoryManager->AddItemToLocalInventory(Item);
            ++Added;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("DevModeDataProvider: Added %d items to inventory"), Added);
}
