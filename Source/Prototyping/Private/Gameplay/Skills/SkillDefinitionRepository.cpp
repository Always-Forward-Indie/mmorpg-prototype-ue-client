#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Engine/DataTable.h"

USkillDefinitionRepository::USkillDefinitionRepository()
{
    SkillDefinitionsTable = nullptr;
    bIsInitialized = false;
}

void USkillDefinitionRepository::Initialize(UDataTable* InSkillDefinitionsTable)
{
    if (!InSkillDefinitionsTable)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDefinitionRepository: Cannot initialize with null DataTable"));
        return;
    }

    SkillDefinitionsTable = InSkillDefinitionsTable;
    LoadDefinitionsFromTable();
    PreloadIconsAsync();
    bIsInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("SkillDefinitionRepository: Initialized with %d skill definitions"), 
        CachedDefinitions.Num());
}

void USkillDefinitionRepository::PreloadIconsAsync()
{
    TArray<FSoftObjectPath> Paths;
    Paths.Reserve(CachedDefinitions.Num());
    for (const auto& Pair : CachedDefinitions)
    {
        const auto& Icon = Pair.Value.skillIcon;
        if (!Icon.IsNull()) Paths.Add(Icon.ToSoftObjectPath());
    }

    if (Paths.Num() == 0) return;

    FStreamableManager& SM = UAssetManager::GetStreamableManager();
    // Держим хендл как UPROPERTY(TObjectPtr<UStreamableRenderAsset> не надо) -> UPROPERTY(TSharedPtr<FStreamableHandle>) не получится,
    // просто сохрани в поле TSharedPtr<FStreamableHandle> PreloadHandle;
    PreloadHandle = SM.RequestAsyncLoad(
        Paths,
        FStreamableDelegate::CreateUObject(this, &USkillDefinitionRepository::OnIconsPreloaded),
        FStreamableManager::AsyncLoadHighPriority
    );
}

void USkillDefinitionRepository::OnIconsPreloaded()
{
    UE_LOG(LogTemp, Log, TEXT("SkillDefinitionRepository: Icons preloaded"));
}

bool USkillDefinitionRepository::HasDefinition(const FString& SkillSlug) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Repository not initialized"));
        return false;
    }

    return CachedDefinitions.Contains(SkillSlug);
}

FSkillDefinitionData USkillDefinitionRepository::GetDefinition(const FString& SkillSlug) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Repository not initialized"));
        return GetDefaultDefinition(SkillSlug);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Getting definition for skill %s"), *SkillSlug);
    
    if (const FSkillDefinitionData* Found = CachedDefinitions.Find(SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Found cached definition for %s - DisplayName: %s, Icon IsValid: %s"), 
            *SkillSlug, 
            *Found->displayName.ToString(),
            Found->skillIcon.IsValid() ? TEXT("true") : TEXT("false"));
            
        if (Found->skillIcon.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Icon path: %s"), 
                *Found->skillIcon.ToSoftObjectPath().ToString());
        }
        
        return *Found;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: No definition found for %s, returning default"), *SkillSlug);
    return GetDefaultDefinition(SkillSlug);
}

TArray<FSkillDefinitionData> USkillDefinitionRepository::GetAllDefinitions() const
{
    TArray<FSkillDefinitionData> Definitions;
    
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Repository not initialized"));
        return Definitions;
    }

    CachedDefinitions.GenerateValueArray(Definitions);
    return Definitions;
}

void USkillDefinitionRepository::RefreshCache()
{
    if (!SkillDefinitionsTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Cannot refresh cache, DataTable is null"));
        return;
    }

    ClearCache();
    LoadDefinitionsFromTable();
    
    UE_LOG(LogTemp, Log, TEXT("SkillDefinitionRepository: Cache refreshed with %d definitions"), 
        CachedDefinitions.Num());
}

void USkillDefinitionRepository::ClearCache()
{
    CachedDefinitions.Empty();
}

void USkillDefinitionRepository::LoadDefinitionsFromTable()
{
    if (!SkillDefinitionsTable)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillDefinitionRepository: SkillDefinitionsTable is null"));
        return;
    }

    // Get all rows from data table
    TArray<FName> RowNames = SkillDefinitionsTable->GetRowNames();
    
    UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Loading from DataTable with %d rows"), RowNames.Num());
    
    for (const FName& RowName : RowNames)
    {
        FSkillDefinitionData* RowData = SkillDefinitionsTable->FindRow<FSkillDefinitionData>(RowName, TEXT("SkillDefinitionRepository"));
        
        if (RowData)
        {
            // Use skillSlug as key, or fallback to row name if skillSlug is empty
            FString SkillSlug = RowData->skillSlug.IsEmpty() ? RowName.ToString() : RowData->skillSlug;
            
            // Ensure skillSlug is set
            if (RowData->skillSlug.IsEmpty())
            {
                RowData->skillSlug = SkillSlug;
            }
            
            // Debug icon loading
            FString IconPath = RowData->skillIcon.ToSoftObjectPath().ToString();
            UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Loading skill %s - Icon path: '%s', IsValid: %s, DisplayName: '%s'"), 
                *SkillSlug,
                *IconPath,
                RowData->skillIcon.IsValid() ? TEXT("true") : TEXT("false"),
                *RowData->displayName.ToString());
            
            CachedDefinitions.Add(SkillSlug, *RowData);
            
            UE_LOG(LogTemp, Verbose, TEXT("SkillDefinitionRepository: Loaded definition for skill %s"), 
                *SkillSlug);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SkillDefinitionRepository: Failed to load row %s"), 
                *RowName.ToString());
        }
    }
}

FSkillDefinitionData USkillDefinitionRepository::GetDefaultDefinition(const FString& SkillSlug) const
{
    FSkillDefinitionData DefaultDefinition;
    DefaultDefinition.skillSlug = SkillSlug;
    DefaultDefinition.displayName = FText::FromString(SkillSlug);
    DefaultDefinition.description = FText::FromString(FString::Printf(TEXT("Skill: %s (No definition found)"), *SkillSlug));
    DefaultDefinition.effectType = ESkillEffectType::None;
    DefaultDefinition.school = ESkillSchool::None;
    DefaultDefinition.skillColor = FLinearColor::Red; // Red to indicate missing definition
    
    return DefaultDefinition;
}