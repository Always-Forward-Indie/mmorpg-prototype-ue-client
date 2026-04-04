#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DataTable.h"
#include "Data/DataStructs.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "SkillDefinitionRepository.generated.h"

/**
 * Repository pattern implementation for skill definitions
 * Handles loading and caching of skill definition data from DataTable
 * Follows Single Responsibility Principle - only manages skill definitions
 */
UCLASS(BlueprintType)
class PROTOTYPING_API USkillDefinitionRepository : public UObject
{
    GENERATED_BODY()

public:
    USkillDefinitionRepository();

    void PreloadIconsAsync();

    void OnIconsPreloaded();

    void PreloadNiagaraAssetsAsync();

    TSharedPtr<FStreamableHandle> PreloadHandle;

    TSharedPtr<FStreamableHandle> NiagaraPreloadHandle;

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Skill Definition Repository")
    void Initialize(UDataTable* InSkillDefinitionsTable);

    // Repository operations
    UFUNCTION(BlueprintCallable, Category = "Skill Definition Repository")
    bool HasDefinition(const FString& SkillSlug) const;

    UFUNCTION(BlueprintCallable, Category = "Skill Definition Repository")
    FSkillDefinitionData GetDefinition(const FString& SkillSlug) const;

    UFUNCTION(BlueprintCallable, Category = "Skill Definition Repository")
    TArray<FSkillDefinitionData> GetAllDefinitions() const;

    // Cache management
    UFUNCTION(BlueprintCallable, Category = "Skill Definition Repository")
    void RefreshCache();

    UFUNCTION(BlueprintCallable, Category = "Skill Definition Repository")
    void ClearCache();

protected:
    // Data table reference
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Repository")
    TObjectPtr<UDataTable> SkillDefinitionsTable;

    // Cached definitions for performance
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Repository")
    TMap<FString, FSkillDefinitionData> CachedDefinitions;

    // Internal methods
    void LoadDefinitionsFromTable();
    FSkillDefinitionData GetDefaultDefinition(const FString& SkillSlug) const;

private:
    bool bIsInitialized = false;
};