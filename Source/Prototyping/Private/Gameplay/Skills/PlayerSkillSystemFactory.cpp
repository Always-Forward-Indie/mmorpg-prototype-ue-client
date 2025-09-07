#include "Gameplay/Skills/PlayerSkillSystemFactory.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"

UPlayerSkillSystemFactory::UPlayerSkillSystemFactory()
{
    CreatedPlayerSkillManager = nullptr;
    CreatedDefinitionRepository = nullptr;
    CreatedNetworkHandler = nullptr;
}

UPlayerSkillManager* UPlayerSkillSystemFactory::CreatePlayerSkillManager(UObject* Outer)
{
    UObject* ValidOuter = GetValidOuter(Outer);
    
    UPlayerSkillManager* SkillManager = NewObject<UPlayerSkillManager>(ValidOuter);
    
    if (SkillManager)
    {
        CreatedPlayerSkillManager = SkillManager;
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillSystemFactory: Created PlayerSkillManager"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Failed to create PlayerSkillManager"));
    }
    
    return SkillManager;
}

USkillDefinitionRepository* UPlayerSkillSystemFactory::CreateSkillDefinitionRepository(UDataTable* SkillDefinitionsTable, UObject* Outer)
{
    if (!SkillDefinitionsTable)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Cannot create SkillDefinitionRepository with null DataTable"));
        return nullptr;
    }

    UObject* ValidOuter = GetValidOuter(Outer);
    
    USkillDefinitionRepository* Repository = NewObject<USkillDefinitionRepository>(ValidOuter);
    
    if (Repository)
    {
        Repository->Initialize(SkillDefinitionsTable);
        CreatedDefinitionRepository = Repository;
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillSystemFactory: Created SkillDefinitionRepository"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Failed to create SkillDefinitionRepository"));
    }
    
    return Repository;
}

UPlayerSkillNetworkHandler* UPlayerSkillSystemFactory::CreatePlayerSkillNetworkHandler(UObject* Outer)
{
    UObject* ValidOuter = GetValidOuter(Outer);
    
    UPlayerSkillNetworkHandler* NetworkHandler = NewObject<UPlayerSkillNetworkHandler>(ValidOuter);
    
    if (NetworkHandler)
    {
        CreatedNetworkHandler = NetworkHandler;
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillSystemFactory: Created PlayerSkillNetworkHandler"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Failed to create PlayerSkillNetworkHandler"));
    }
    
    return NetworkHandler;
}

bool UPlayerSkillSystemFactory::CreateCompletePlayerSkillSystem(
    USkillSystemManager* SkillSystemManager,
    UNetworkManager* NetworkManager,
    UDataTable* SkillDefinitionsTable,
    UTimeSyncService* TimeSyncService,
    UObject* Outer)
{
    // Validate dependencies
    if (!ValidateFactoryDependencies(SkillSystemManager, NetworkManager))
    {
        return false;
    }

    if (!SkillDefinitionsTable)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: SkillDefinitionsTable is required"));
        return false;
    }

    UObject* ValidOuter = GetValidOuter(Outer);

    // Create all components
    USkillDefinitionRepository* Repository = CreateSkillDefinitionRepository(SkillDefinitionsTable, ValidOuter);
    if (!Repository)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Failed to create SkillDefinitionRepository"));
        return false;
    }

    UPlayerSkillManager* SkillManager = CreatePlayerSkillManager(ValidOuter);
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Failed to create PlayerSkillManager"));
        return false;
    }

    UPlayerSkillNetworkHandler* NetworkHandler = CreatePlayerSkillNetworkHandler(ValidOuter);
    if (!NetworkHandler)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: Failed to create PlayerSkillNetworkHandler"));
        return false;
    }

    // Initialize components with dependencies
    SkillManager->Initialize(SkillSystemManager, Repository, TimeSyncService);
    NetworkHandler->Initialize(SkillManager, NetworkManager);

    // Set up integration with existing skill system
    SkillSystemManager->SetPlayerSkillManager(SkillManager);

    // Subscribe to network events
    NetworkHandler->SubscribeToNetworkEvents();

    UE_LOG(LogTemp, Warning, TEXT("PlayerSkillSystemFactory: Created complete player skill system successfully"));
    return true;
}

void UPlayerSkillSystemFactory::CleanupCreatedComponents()
{
    if (CreatedNetworkHandler)
    {
        CreatedNetworkHandler->UnsubscribeFromNetworkEvents();
        CreatedNetworkHandler = nullptr;
    }

    if (CreatedPlayerSkillManager)
    {
        CreatedPlayerSkillManager = nullptr;
    }

    if (CreatedDefinitionRepository)
    {
        CreatedDefinitionRepository->ClearCache();
        CreatedDefinitionRepository = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("PlayerSkillSystemFactory: Cleaned up created components"));
}

UObject* UPlayerSkillSystemFactory::GetValidOuter(UObject* ProvidedOuter) const
{
    if (ProvidedOuter)
    {
        return ProvidedOuter;
    }
    
    // Fallback to this factory as outer
    return const_cast<UPlayerSkillSystemFactory*>(this);
}

bool UPlayerSkillSystemFactory::ValidateFactoryDependencies(USkillSystemManager* SkillSystemManager, UNetworkManager* NetworkManager) const
{
    if (!SkillSystemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: SkillSystemManager is required"));
        return false;
    }

    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: NetworkManager is required"));
        return false;
    }

    if (!IsValid(SkillSystemManager))
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: SkillSystemManager is not valid"));
        return false;
    }

    if (!IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillSystemFactory: NetworkManager is not valid"));
        return false;
    }

    return true;
}