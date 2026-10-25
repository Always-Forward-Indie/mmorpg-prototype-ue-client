// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Components/CapsuleComponent.h"
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


void UMyGameInstance::SpawnPlayerForClient(int32 ClientID)
{
	if (!ConnectedPlayers.Contains(ClientID))
	{
		UE_LOG(LogConnection, Error, TEXT("SpawnPlayerForClient: ClientID %d not in ConnectedPlayers"), ClientID);
		return;
	}

	if (SpawnedPlayers.Contains(ClientID))
	{
		ABasicPlayer* Existing = SpawnedPlayers[ClientID];
		if (IsValid(Existing) && !Existing->IsActorBeingDestroyed())
		{
			UE_LOG(LogConnection, Warning, TEXT("SpawnPlayerForClient: ClientID %d already spawned, skipping"), ClientID);
			return;
		}
		SpawnedPlayers.Remove(ClientID);
	}

	FClientDataStruct PlayerData = ConnectedPlayers[ClientID];
	const bool bIsLocal = (ClientID == CurrentClientID);

	FVector SpawnLocation(
		PlayerData.characterData.characterPosition.positionX,
		PlayerData.characterData.characterPosition.positionY,
		PlayerData.characterData.characterPosition.positionZ);
	FRotator SpawnRotation(0.0f, PlayerData.characterData.characterPosition.rotationZ, 0.0f);

	// Use SpawnActorDeferred so we can set isOtherClient BEFORE BeginPlay fires.
	// This is critical for remote players: BeginPlay reads playerData.isOtherClient
	// to initialize interpolation targets at the correct spawn location.
	// With regular SpawnActor, isOtherClient is still false during BeginPlay,
	// causing LastReceivedPosition to be initialised to (0,0,0) instead of SpawnLocation.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABasicPlayer* NewPlayer = GetWorld()->SpawnActorDeferred<ABasicPlayer>(MainPlayerClass, FTransform(SpawnRotation, SpawnLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewPlayer)
	{
		UE_LOG(LogConnection, Error, TEXT("SpawnPlayerForClient: Failed to spawn ABasicPlayer for ClientID %d"), ClientID);
		return;
	}

	// Set isOtherClient and IDs BEFORE FinishSpawning (which calls BeginPlay).
	NewPlayer->SetIsOtherClient(!bIsLocal);
	NewPlayer->SetClientID(PlayerData.clientId);
	NewPlayer->SetCharacterID(PlayerData.characterData.characterId);

	// FinishSpawning triggers BeginPlay — at this point isOtherClient is already correct,
	// so BeginPlay initialises LastReceivedPosition = SpawnLocation (not local player pos).
	UGameplayStatics::FinishSpawningActor(NewPlayer, FTransform(SpawnRotation, SpawnLocation));

	UE_LOG(LogConnection, Warning, TEXT("SpawnPlayerForClient: Spawned ClientID=%d CharID=%d IsLocal=%d Pos=(%.0f,%.0f,%.0f)"),
		ClientID, PlayerData.characterData.characterId, bIsLocal,
		SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);

	// SetCoordinates is now a no-op for first-packet snap (bHasReceivedFirstPosition
	// was set true inside BeginPlay when isOtherClient=true and position = SpawnLocation).
	// We still call it to sync playerData fields and interpolation targets.
	NewPlayer->SetCoordinates(
		PlayerData.characterData.characterPosition.positionX,
		PlayerData.characterData.characterPosition.positionY,
		PlayerData.characterData.characterPosition.positionZ,
		PlayerData.characterData.characterPosition.rotationZ,
		PlayerData.characterData.bIsFalling);
	NewPlayer->SetPlayerName(PlayerData.characterData.characterName);
	NewPlayer->SetPlayerClass(PlayerData.characterData.characterClass);
	NewPlayer->SetPlayerRace(PlayerData.characterData.characterRace);
	NewPlayer->SetPlayerGender(PlayerData.characterData.characterGender);
	NewPlayer->SetPlayerLevel(PlayerData.characterData.characterLevel);
	NewPlayer->SetPlayerNextLevelExp(PlayerData.characterData.characterExpForLevelEnd);
	NewPlayer->SetPlayerExpPoints(PlayerData.characterData.characterExperiencePoints);
	NewPlayer->SetPlayerCurrentHPPoints(PlayerData.characterData.characterCurrentHealth);
	NewPlayer->SetPlayerCurrentMPPoints(PlayerData.characterData.characterCurrentMana);
	NewPlayer->SetPlayerAttributes(PlayerData.characterData.characterAttributes.attributesData);
	NewPlayer->SetPlayerTag(*FString::FromInt(PlayerData.characterData.characterId));
	NewPlayer->SetPlayerTag(TEXT("Player"));

	// Apply visual from DataTable (mesh, animation, scale based on class/race/gender)
	if (CharacterVisualDefinitionsTable)
	{
		NewPlayer->ApplyVisualFromDataTable(CharacterVisualDefinitionsTable);
	}

	// Apply dead state from server before UI is shown so the death screen
	// appears immediately if the character was dead at login.
	if (PlayerData.characterData.bIsDead)
	{
		NewPlayer->SetDead_Implementation(true);
		UE_LOG(LogConnection, Warning, TEXT("SpawnPlayerForClient: CharID=%d spawned as DEAD"), PlayerData.characterData.characterId);
	}

	if (CombatSystemManager && PlayerData.characterData.characterId > 0 &&
		IsValid(NewPlayer) && !NewPlayer->IsActorBeingDestroyed())
	{
		TScriptInterface<ICombatable> CombatableInterface;
		CombatableInterface.SetObject(NewPlayer);
		CombatableInterface.SetInterface(Cast<ICombatable>(NewPlayer));
		CombatSystemManager->RegisterCombatable(CombatableInterface);
		UE_LOG(LogConnection, Warning, TEXT("SpawnPlayerForClient: Registered CharID=%d with CombatSystem"), PlayerData.characterData.characterId);
	}

	if (MOBManager && PlayerData.characterData.characterId > 0)
	{
		MOBManager->RegisterPlayer(PlayerData.characterData.characterId, NewPlayer);
	}

	SpawnedPlayers.Add(ClientID, NewPlayer);

	// Initialize nameplate for both local and remote players
	NewPlayer->InitialiseNameplate(bIsLocal);

	// Remote players must not block the local player's camera spring arm.
	// Disable ECC_Camera response on both the capsule and the skeletal mesh so
	// the camera never zooms in when another player walks in front of it.
	if (!bIsLocal)
	{
		if (UCapsuleComponent* Cap = NewPlayer->GetCapsuleComponent())
		{
			Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		if (USkeletalMeshComponent* Mesh = NewPlayer->GetMesh())
		{
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
	}

	// Apply cached move_speed immediately so the player has the correct MaxWalkSpeed
	// from frame 1. stats_update packets arrive before the actor is spawned (world travel),
	// so the authoritative speed sits in PlayerStatsManager cache — apply it now.
	if (bIsLocal && PlayerStatsManager)
	{
		FStatAttributeEntry SpeedAttr;
		if (PlayerStatsManager->GetAttribute(TEXT("move_speed"), SpeedAttr) && SpeedAttr.effective > 0.f)
		{
			NewPlayer->ApplyServerMoveSpeed(SpeedAttr.effective);
			UE_LOG(LogConnection, Verbose, TEXT("SpawnPlayerForClient: Applied cached move_speed=%.1f to CharID=%d"),
				SpeedAttr.effective, PlayerData.characterData.characterId);
		}
	}

	if (bIsLocal)
	{
		Player = NewPlayer;
		Player->SetIsOtherClient(false);
		NewPlayer->SetClientLogin(PlayerData.clientLogin);
		NewPlayer->SetClientSecret(PlayerData.hash);

		// GetFirstLocalPlayerController() is on UGameInstance and is PIE-safe:
		// returns the controller owned by THIS GameInstance's LocalPlayer,
		// not always Player 1's controller.
		APlayerController* PC = GetFirstLocalPlayerController(GetWorld());
		if (PC)
		{
		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 3b. Calling PC->Possess(Player) � camera will switch NOW"));
		PC->Possess(Player);
		{
			FRotator CR = PC->GetControlRotation();
			FVector  CamLoc = Player->GetFollowCamera() ? Player->GetFollowCamera()->GetComponentLocation() : FVector::ZeroVector;
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 3c. PC->Possess done. Pawn=%s ControlRot=(P=%.1f Y=%.1f R=%.1f) CameraLoc=(%.0f,%.0f,%.0f)"),
				PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("NULL"),
				CR.Pitch, CR.Yaw, CR.Roll,
				CamLoc.X, CamLoc.Y, CamLoc.Z);
		}
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("SpawnPlayerForClient: No local PlayerController for ClientID=%d"), ClientID);
		}

		if (InventoryManager)
		{
			// Stamp the owner character ID on the global InventoryManager so it only
			// processes packets for our local character, not other players on the same feed.
			InventoryManager->SetOwnerCharacterId(PlayerData.characterData.characterId);
			// Prefetch only: races the frag handshake, so its response may be
			// stale. The authoritative refresh happens on playerReady ACK
			// (NotifyPlayerReadyAck, S13-C2). Keep both — this one fills the
			// UI early, the ACK one guarantees correctness.
			InventoryManager->RequestInventoryData(PlayerData.characterData.characterId);
			UE_LOG(LogConnection, Warning, TEXT("SpawnPlayerForClient: Requested inventory for CharID=%d"), PlayerData.characterData.characterId);
		}
	}
	else
	{
		// Remote player: bind equipment visuals to PLAYER_EQUIPMENT_UPDATE packets,
		// filtered per character ID so each remote player only sees its own updates.
		UEquipmentVisualComponent* VisComp = NewPlayer->GetEquipmentVisualComponent();
		if (VisComp && EquipmentManager && ItemManager)
		{
			const int32 RemoteCharId = PlayerData.characterData.characterId;
			VisComp->SetOwnerCharacterId(RemoteCharId);
			VisComp->InitializeForRemotePlayer(ItemManager);
			EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.AddDynamic(
				VisComp, &UEquipmentVisualComponent::HandleRemoteEquipmentState);
			UE_LOG(LogConnection, Verbose, TEXT("SpawnPlayerForClient: Equipment visuals bound for remote CharID=%d"), RemoteCharId);

			// Bind cosmetic visual component to remote equipment updates so that
			// helmets correctly hide hair for other players on the same screen.
			if (UCosmeticVisualComponent* CosmeticVis = NewPlayer->GetCosmeticVisualComponent())
			{
				CosmeticVis->SetOwnerCharacterId(RemoteCharId);
				EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.AddDynamic(
					CosmeticVis, &UCosmeticVisualComponent::HandleRemoteEquipmentState);
			}

			// Replay any PLAYER_EQUIPMENT_UPDATE that arrived before this actor was spawned.
			// This resolves the race where the server broadcasts equipment immediately after
			// playerReady but the client is still deferred in PendingRemotePlayerSpawns.
			if (const FEquipmentStateData* Cached = EquipmentManager->GetCachedRemoteEquipmentState(RemoteCharId))
			{
				if (Cached->slots.Num() > 0)
				{
					VisComp->HandleRemoteEquipmentState(*Cached);
					UE_LOG(LogConnection, Verbose, TEXT("SpawnPlayerForClient: Replayed cached equipment for remote CharID=%d (%d slot(s))"),
						RemoteCharId, Cached->slots.Num());

					// Also replay to cosmetics so hair-hide is applied from the cached state.
					if (UCosmeticVisualComponent* CosmeticVis = NewPlayer->GetCosmeticVisualComponent())
					{
						CosmeticVis->HandleRemoteEquipmentState(*Cached);
		}
	}

	if (bReturningToLogin)
	{
		UE_LOG(LogConnection, Verbose, TEXT("OnStart: Returning to login, loading login level"));
		LoadLoginLevel();
	}
}

			// NOTE: Do NOT call RequestGetEquipment(RemoteCharId) here — the server
			// resolves getEquipment requests by session character ID (i.e. the LOCAL
			// player's character), so the request would return OUR OWN equipment, not
			// the remote player's.  The server now broadcasts the new player's equipment
			// to all existing clients via broadcastEquipmentUpdate inside
			// handlePlayerReadyEvent, so the PLAYER_EQUIPMENT_UPDATE will arrive
			// automatically and be routed here through OnRemoteEquipmentStateReceivedDelegate.
		}

		// Apply initial equipped title to this remote player's nameplate.
		// The server includes equippedTitleDisplayName (and equippedTitleSlug) in
		// joinGameCharacter/getConnectedCharacters.
		if (!PlayerData.characterData.equippedTitleSlug.IsEmpty())
		{
			FText TitleText;
			if (const ULocalizationSubsystem* Loc = GetSubsystem<ULocalizationSubsystem>())
			{
				TitleText = Loc->GetTitleDisplayName(PlayerData.characterData.equippedTitleSlug);
			}
			if (TitleText.IsEmpty() && !PlayerData.characterData.equippedTitleDisplayName.IsEmpty())
			{
				TitleText = FText::FromString(PlayerData.characterData.equippedTitleDisplayName);
			}
			if (TitleText.IsEmpty())
			{
				TitleText = FText::FromString(PlayerData.characterData.equippedTitleSlug);
			}
			NewPlayer->SetEquippedTitle(TitleText);
		}
	}
}
