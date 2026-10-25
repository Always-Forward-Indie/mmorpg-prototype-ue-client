// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "Components/ArrowComponent.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "DevMode/DevModeDataProvider.h"
#include "DevMode/DevModeConsoleCommands.h"
#include "DevMode/LiveBotDriver.h"
#include "Containers/Ticker.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Combat/DamageEffectHandler.h"
#include "Gameplay/Combat/HealingEffectHandler.h"
#include "Gameplay/Combat/BuffEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/ExperienceNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/MasteryManager.h"
#include "Gameplay/Player/MasteryNetworkHandler.h"
#include "Gameplay/Player/ReputationManager.h"
#include "Gameplay/Player/ReputationNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Gameplay/NPCs/AmbientSpeechNetworkHandler.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Gameplay/WorldObjects/WIONetworkHandler.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Animation/AnimNotify_Footstep.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Gameplay/Skills/PlayerSkillSystemFactory.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Services/TimeSyncService.h"
#include "Networking/PingManager.h"
#include "UI/UIManager.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/NPCs/NPCNetworkHandler.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Dialogue/DialogueNetworkHandler.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Quest/QuestNetworkHandler.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Trade/TradeNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Vendor/VendorNetworkHandler.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Repair/RepairNetworkHandler.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/SkillShop/SkillShopNetworkHandler.h"
#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "Gameplay/LoginLevel/LoginLevelSetupActor.h"

#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Chat/ChatNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "UObject/UObjectGlobals.h"
#include "Audio/MusicZoneActor.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"


void UMyGameInstance::SetCurrentClientID(int32 ClientID)
{
	CurrentClientID = ClientID;

	// set client ID to client data
	ClientData.clientId = ClientID;
}

int32 UMyGameInstance::GetCurrentClientID()
{
	return CurrentClientID;
}

void UMyGameInstance::SetCurrentClientHash(FString ClientSecret)
{
	CurrentClientSecret = ClientSecret;

	// set client token to client data
	ClientData.hash = ClientSecret;
}

FString UMyGameInstance::GetCurrentClientHash()
{
	return CurrentClientSecret;
}

void UMyGameInstance::SetCurrentCharacterID(int32 CharacterID)
{
	CurrentCharacterID = CharacterID;
    
    // Update the character ID in the client data
    ClientData.characterData.characterId = CharacterID;
    
    UE_LOG(LogConnection, Warning, TEXT("SetCurrentCharacterID %d"), CharacterID);
}

int32 UMyGameInstance::GetCurrentCharacterID()
{
	return CurrentCharacterID;
}

void UMyGameInstance::JoinSelectedCharacterToGame()
{
	UE_LOG(LogConnection, Verbose, TEXT("JoinSelectedCharacterToGame called with CharacterID: %d"), CurrentCharacterID);

	if (!JoinGuard.TryEnter())
	{
		UE_LOG(LogConnection, Verbose, TEXT("JoinSelectedCharacterToGame: join already in progress, ignoring duplicate call (charId=%d)"), CurrentCharacterID);
		return;
	}

	if (CurrentCharacterID <= 0)
	{
		UE_LOG(LogConnection, Error, TEXT("Cannot join game: No character selected."));
		return;
	}

	if (CurrentClientID <= 0 || CurrentClientSecret.IsEmpty())
	{
		UE_LOG(LogConnection, Error, TEXT("Cannot join game: Client not authenticated."));
		return;
	}

	// PRE-PHASE: Send joinGameClient to Game Server first (latch held by TryEnter above).
	// The Game Server will respond with chunkServerData, and the response handler
	// (ProcessGameServerData) will chain to Phase 1 (joinGameClient on Chunk Server),
	// then Phase 1 continued (joinGameCharacter on Chunk Server).
	if (PlayerManager)
	{
		UE_LOG(LogGameplay, Warning, TEXT("PRE-PHASE: Sending joinGameClient to Game Server for character %d"), CurrentCharacterID);
		PlayerManager->SendJoinGameRequest(ClientData);
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("Cannot join game: PlayerManager is null."));
		JoinGuard.Reset();
	}
}

void UMyGameInstance::AddPlayerData(int32 ClientID, const FClientDataStruct clientData)
{
	if (ConnectedPlayers.Contains(ClientID))
	{
		// Update existing entry so a reconnecting player gets fresh data.
		// If the player actor is already spawned we leave it in place;
		// SpawnPlayerForClient guards against double-spawning.
		ConnectedPlayers[ClientID] = clientData;
		UE_LOG(LogConnection, Verbose, TEXT("AddPlayerData: Updated existing entry for ClientID=%d CharID=%d"),
			clientData.clientId, clientData.characterData.characterId);
	}
	else
	{
		UE_LOG(LogConnection, Verbose, TEXT("AddPlayerData: Added ClientID=%d CharID=%d"),
			clientData.clientId, clientData.characterData.characterId);
		ConnectedPlayers.Add(ClientID, clientData);
	}
}

void UMyGameInstance::RemovePlayerData(int32 ClientID)
{
	// Check if the player ID exists
	if (ConnectedPlayers.Contains(ClientID))
	{
		ConnectedPlayers.Remove(ClientID);

		// If the player was spawned, clean up delegate bindings before destroying
		if (SpawnedPlayers.Contains(ClientID))
		{
			ABasicPlayer* PlayerToRemove = SpawnedPlayers[ClientID];
			if (IsValid(PlayerToRemove) && !PlayerToRemove->IsActorBeingDestroyed())
			{
				// Unsubscribe remote equipment visual component from the delegate
				// to prevent dangling callbacks after actor destruction.
				UEquipmentVisualComponent* VisComp = PlayerToRemove->GetEquipmentVisualComponent();
				if (VisComp && EquipmentManager)
				{
					EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.RemoveDynamic(
						VisComp, &UEquipmentVisualComponent::HandleRemoteEquipmentState);
				}

				// Unsubscribe cosmetic visual component from the same remote delegate.
				UCosmeticVisualComponent* CosmeticVis = PlayerToRemove->GetCosmeticVisualComponent();
				if (CosmeticVis && EquipmentManager)
				{
					EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.RemoveDynamic(
						CosmeticVis, &UCosmeticVisualComponent::HandleRemoteEquipmentState);
				}

				// Unregister from combat system
				if (CombatSystemManager)
				{
					TScriptInterface<ICombatable> CombatableInterface;
					CombatableInterface.SetObject(PlayerToRemove);
					CombatableInterface.SetInterface(Cast<ICombatable>(PlayerToRemove));
					CombatSystemManager->UnregisterCombatable(CombatableInterface);
				}

				// Unregister from MOBManager player registry
				if (MOBManager)
				{
					const int32 CharId = PlayerToRemove->GetPlayerCharacterID();
					if (CharId > 0)
					{
						MOBManager->UnregisterPlayer(CharId);
					}
				}

				// Deregister mesh from Nanite/render pipeline before GC destroys the actor.
				// Without this, the RHI thread can hold a pending Nanite::Readback on the
				// mesh resources while the game thread frees them — causing a use-after-free
				// crash in Nanite::Readback / SceneRender.
				PlayerToRemove->SetActorHiddenInGame(true);
				PlayerToRemove->SetActorEnableCollision(false);
				if (USkeletalMeshComponent* Mesh = PlayerToRemove->GetMesh())
				{
					Mesh->SetVisibility(false);
				}

				PlayerToRemove->Destroy();
			}
			SpawnedPlayers.Remove(ClientID);
		}
	}
}

void UMyGameInstance::MovePlayerForClient(const int32 ClientID, const FClientDataStruct& clientData, const FMessageDataStruct& MessageData)
{
	// If the player has been spawned and it's not the current client
	if (SpawnedPlayers.Contains(ClientID) && ClientID != CurrentClientID)
	{
		ABasicPlayer* PlayerToMove = SpawnedPlayers[ClientID];
		if (PlayerToMove)
		{
			FString CurrentTimestamp = MessageData.timestamp;

			//set character coordinates
		PlayerToMove->SetCoordinates(clientData.characterData.characterPosition.positionX,
			clientData.characterData.characterPosition.positionY,
			clientData.characterData.characterPosition.positionZ,
			clientData.characterData.characterPosition.rotationZ,
			clientData.characterData.bIsFalling);

			// Bug 10 fix: if the remote player is still marked dead but is now sending
			// movement updates, they must have respawned on the server side.
			// Auto-revive them so their dead animation and disabled movement are cleared.
			// (respawnResult is sent only to the respawning player's client; other clients
			// detect the respawn here when position packets resume after death.)
			if (PlayerToMove->GetIsDead())
			{
				PlayerToMove->SetDead_Implementation(false);
				// stats_update after respawn is unicast to the respawning player only,
				// so other clients cannot know the exact new HP.  Restore to max as a
				// best-effort approximation — the nameplate will be corrected if a
				// subsequent charAttributesUpdate or stats broadcast arrives.
				const int32 MaxHP = PlayerToMove->GetMaxHealth_Implementation();
				PlayerToMove->SetPlayerCurrentHPPoints(MaxHP);
				UE_LOG(LogConnection, Verbose, TEXT("MovePlayerForClient: Auto-revived remote player CharID=%d HP set to %d"),
					clientData.characterData.characterId, MaxHP);
			}
		}
	}
}

void UMyGameInstance::HandlePlayerDisconnection(int32 ClientID)
{
	// Remove the player from our map and destroy the actor if it exists
	RemovePlayerData(ClientID);
}

void UMyGameInstance::UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z, double rotZ)
{
	// Find the player in the map
	if (ConnectedPlayers.Contains(PlayerID))
	{
		FClientDataStruct& PlayerData = ConnectedPlayers[PlayerID];

		// Update the player's position in our map
		PlayerData.characterData.characterPosition.positionX = x;
		PlayerData.characterData.characterPosition.positionY = y;
		PlayerData.characterData.characterPosition.positionZ = z;
		PlayerData.characterData.characterPosition.rotationZ = rotZ;

		// If the player has been spawned, update its position in the world
		if (SpawnedPlayers.Contains(PlayerID))
		{
			ABasicPlayer* PlayerToMove = SpawnedPlayers[PlayerID];
			if (PlayerToMove)
			{
				FVector NewLocation(x, y, z);
				FRotator NewRotation(0.0f, rotZ, 0.0f);

				PlayerToMove->SetActorLocationAndRotation(NewLocation, NewRotation);
			}
		}
	}
}

// Set character items to list view widget
void UMyGameInstance::SetCharacterItems(TArray<FCharacterDataStruct> Items)
{
	if (!LoginScreenWidget)
	{
		// Live-bot flow (headless auto-login) intentionally has no login UI:
		// the driver consumes the character list directly. Quiet Verbose there;
		// a null widget in the manual flow is still an Error (empty list).
		if (!LiveBotScenario.IsEmpty())
		{
			UE_LOG(LogGameplay, Verbose, TEXT("SetCharacterItems: no login widget in live-bot flow, items consumed by driver"));
			return;
		}
		UE_LOG(LogGameplay, Error, TEXT("Failed to set character items - LoginScreenWidget is null"));
		return;
	}

	UListView* CharacterListView = LoginScreenWidget->GetCharactersListView();

	if (CharacterListView)
	{
		// Clear existing items first
		CharacterListView->ClearListItems();
		
		// Populate the list with character items
		for (const FCharacterDataStruct& Character : Items)
		{
			UCharacterListItem* CharacterListItemWidget = CreateWidget<UCharacterListItem>(this, CharactersListItemWidgetClass);

			// Check if the widget creation was successful
			if (CharacterListItemWidget)
			{
				// Set character data in the list item widget
				CharacterListItemWidget->SetCharacterItemData(Character.characterName, Character.characterId);
				
				// Add the item to the List View
				CharacterListView->AddItem(CharacterListItemWidget);
				
				UE_LOG(LogGameplay, Warning, TEXT("Added character to list: %s (ID: %d)"), 
					*Character.characterName, Character.characterId);
			}
		}
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("Failed to set character items - LoginScreenWidget or CharacterListView is null"));
	}
}

// Combat system functions
ABasicPlayer* UMyGameInstance::GetPlayerByCharacterId(int32 CharacterId)
{
	// Check if we have a local player with this character ID
	if (Player && Player->GetPlayerCharacterID() == CharacterId)
	{
		return Player;
	}

	// Check spawned players for the character ID
	for (auto& PlayerPair : SpawnedPlayers)
	{
		ABasicPlayer* PlayerActor = PlayerPair.Value;
		if (PlayerActor && IsValid(PlayerActor) && PlayerActor->GetPlayerCharacterID() == CharacterId)
		{
			return PlayerActor;
		}
	}

	// If not found, return nullptr
	UE_LOG(LogGameplay, Warning, TEXT("MyGameInstance: Player with character ID %d not found"), CharacterId);
	return nullptr;
}
