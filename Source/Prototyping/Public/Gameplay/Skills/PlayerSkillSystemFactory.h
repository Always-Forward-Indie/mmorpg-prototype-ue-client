#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DataTable.h"
#include "PlayerSkillSystemFactory.generated.h"

// Forward declarations
class UPlayerSkillManager;
class USkillDefinitionRepository;
class UPlayerSkillNetworkHandler;
class USkillSystemManager;
class UNetworkManager;
class UTimeSyncService;

/**
 * Factory pattern implementation for creating player skill system components
 * Follows SOLID principles and provides clean dependency injection
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UPlayerSkillSystemFactory : public UObject
{
    GENERATED_BODY()

public:
    UPlayerSkillSystemFactory();

    // Factory methods
    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    UPlayerSkillManager* CreatePlayerSkillManager(UObject* Outer = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    USkillDefinitionRepository* CreateSkillDefinitionRepository(UDataTable* SkillDefinitionsTable, UObject* Outer = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    UPlayerSkillNetworkHandler* CreatePlayerSkillNetworkHandler(UObject* Outer = nullptr);

    // Complete system creation
    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    bool CreateCompletePlayerSkillSystem(
        USkillSystemManager* SkillSystemManager,
        UNetworkManager* NetworkManager,
        UDataTable* SkillDefinitionsTable,
        UTimeSyncService* TimeSyncService = nullptr,
        UObject* Outer = nullptr
    );

    // Get created components
    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    UPlayerSkillManager* GetPlayerSkillManager() const { return CreatedPlayerSkillManager; }

    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    USkillDefinitionRepository* GetSkillDefinitionRepository() const { return CreatedDefinitionRepository; }

    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    UPlayerSkillNetworkHandler* GetPlayerSkillNetworkHandler() const { return CreatedNetworkHandler; }

    // Cleanup
    UFUNCTION(BlueprintCallable, Category = "Player Skill System Factory")
    void CleanupCreatedComponents();

protected:
    // Created components (for tracking and cleanup)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Created Components")
    TObjectPtr<UPlayerSkillManager> CreatedPlayerSkillManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Created Components")
    TObjectPtr<USkillDefinitionRepository> CreatedDefinitionRepository;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Created Components")
    TObjectPtr<UPlayerSkillNetworkHandler> CreatedNetworkHandler;

private:
    // Internal helper methods
    UObject* GetValidOuter(UObject* ProvidedOuter) const;
    bool ValidateFactoryDependencies(USkillSystemManager* SkillSystemManager, UNetworkManager* NetworkManager) const;
};