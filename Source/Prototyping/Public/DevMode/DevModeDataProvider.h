#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Dom/JsonObject.h"
#include "DevModeDataProvider.generated.h"

class UMyGameInstance;
class UInventoryManager;
class UMOBManager;
class UPlayerStatsManager;
class UExperienceManager;

// ============================================================================
// DevMode configuration struct - editable in Blueprint defaults on GameInstance
// ============================================================================
USTRUCT(BlueprintType)
struct FDevModeConfig
{
    GENERATED_BODY()

    // Master switch. When true the game skips Login/Auth and goes straight to the level.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode")
    bool bEnabled = false;

    // Level to open. If NAME_None, falls back to GameLevelName on the GameInstance.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode")
    FName LevelOverride = NAME_None;

    // Path to dev_player.json inside the project Content or Config directory.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode|Data")
    FString PlayerDataJsonPath = TEXT("Config/DevMode/dev_player.json");

    // Path to dev_mobs.json
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode|Data")
    FString MobDataJsonPath = TEXT("Config/DevMode/dev_mobs.json");

    // Path to dev_inventory.json
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode|Data")
    FString InventoryDataJsonPath = TEXT("Config/DevMode/dev_inventory.json");

    // Whether to spawn test mobs after level load.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode|Mobs")
    bool bSpawnTestMobs = true;

    // Whether to populate the inventory with test data after level load.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode|Inventory")
    bool bPopulateInventory = true;
};

// ============================================================================
// DevModeDataProvider
// Reads JSON files from disk and feeds fake data into the game managers
// so all gameplay systems work without a live server connection.
// ============================================================================
UCLASS()
class PROTOTYPING_API UDevModeDataProvider : public UObject
{
    GENERATED_BODY()

public:
    // Call once after managers are created. GameInstance is used for world context.
    void Initialize(UMyGameInstance* InGameInstance, const FDevModeConfig& InConfig);

    // Build a fake FClientDataStruct from dev_player.json.
    // Returns false and logs an error if the file cannot be read.
    bool LoadPlayerData(FClientDataStruct& OutClientData) const;

    // Spawn test mobs into the world using dev_mobs.json.
    void PopulateMobs(UMOBManager* InMobManager) const;

    // Inject fake inventory rows into the InventoryManager.
    void PopulateInventory(UInventoryManager* InInventoryManager, int32 CharacterId) const;

private:
    // Read a JSON file from the absolute path, return raw string or empty on error.
    FString ReadJsonFile(const FString& RelativePath) const;

    // Build the absolute path from a project-relative path.
    FString ResolveFilePath(const FString& RelativePath) const;

    // Parse a single mob entry from a JSON object.
    bool ParseMobEntry(const TSharedPtr<FJsonObject>& JsonObj, FMOBStruct& OutMob) const;

    // Parse a single inventory item entry from a JSON object.
    bool ParseInventoryItem(const TSharedPtr<FJsonObject>& JsonObj, FInventoryItemStruct& OutItem,
                            int32 CharacterId) const;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    FDevModeConfig Config;
};
