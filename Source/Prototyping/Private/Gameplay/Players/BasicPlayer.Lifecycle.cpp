// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "UI/UIManager.h"
#include "UI/NotificationToastWidget.h"
#include "UI/ChatWidget.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/ItemStruct.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Utils/PlayerAttributeParser.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/UI/CastBarWidget.h"
#include "Gameplay/Combat/BaseMMOProjectile.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Data/EffectDefinitionTable.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "Data/CharacterVisualData.h"
#include "Engine/AssetManager.h"

void ABasicPlayer::BeginPlay()
{
Super::BeginPlay();

   // Initialize interpolation targets to the actual spawn location so that
   // UpdateRemotePlayerMovement never sees uninitialized (0,0,0) values before
   // the first SetCoordinates call arrives from the network.
   {
       const FVector SpawnLoc = GetActorLocation();
       LastReceivedPosition   = SpawnLoc;
       TargetReceivedPosition = SpawnLoc;
       LastReceivedRotation   = GetActorRotation();
       TargetReceivedRotation = GetActorRotation();
   }

   MyGameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this));
   if (MyGameInstance)
   {
       UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] BasicPlayer::BeginPlay вЂ” CharID=%d isOtherClient=%d Pos=(%.0f,%.0f,%.0f)"),
           playerData.characterData.characterId, (int)playerData.isOtherClient,
           GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
        UE_LOG(LogTemp, Warning, TEXT("GameInstance found"));

        // Route the player's AudioComponent through the SFX SoundClass
        if (AudioComponent && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
        {
            AudioComponent->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
        }

        // Register in O(1) actor registry so FindTargetActor skips GetAllActorsOfClass
        const int32 ActorId = GetActorId_Implementation();
        if (MyGameInstance->MOBManager && ActorId > 0)
        {
            MyGameInstance->MOBManager->RegisterPlayer(ActorId, this);
        }

        // Remote players: disable physics push вЂ” positions are server-authoritative.
        // Prevents remote capsules from physically blocking the local player's movement.
        // Projectile hits still work because ECC_WorldDynamic is set to Overlap.
        if (playerData.isOtherClient)
        {
            if (UCharacterMovementComponent* CMC = GetCharacterMovement())
            {
                CMC->bEnablePhysicsInteraction = false;
            }
            if (UCapsuleComponent* Capsule = GetCapsuleComponent())
            {
                Capsule->SetCollisionResponseToChannel(ECC_Pawn,         ECR_Ignore);
                Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
            }
        }

        // Register with combat system if this is the local player
        if (!playerData.isOtherClient)
        {
            UCombatSystemManager* CombatManager = MyGameInstance->GetCombatSystemManager();
            if (CombatManager && GetActorId_Implementation() > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->RegisterCombatable(CombatableInterface);
                UE_LOG(LogTemp, Warning, TEXT("Player %d registered with combat system"), GetActorId_Implementation());
            }
        }
    }
    else
    {
		UE_LOG(LogTemp, Error, TEXT("GameInstance not found"));
	}

	UE_LOG(LogTemp, Warning, TEXT("Player Was Created"));

	// Bind emote-ended delegate so movement unlocks when the emote finishes
	if (!playerData.isOtherClient && EmoteComponent)
	{
		EmoteComponent->OnEmoteEnded.AddDynamic(this, &ABasicPlayer::HandleEmoteEnded);
	}

	// Initialize inventory manager for local player only
	if (!playerData.isOtherClient && MyGameInstance)
	{
		// Set up inventory manager
		if (InventoryManager)
		{
			InventoryManager->SetWorldContext(GetWorld());
			InventoryManager->SetGameInstance(MyGameInstance);

			// Stamp owner character ID BEFORE subscribing so packet filter is active from the first message.
			if (playerData.characterData.characterId > 0)
			{
				InventoryManager->SetOwnerCharacterId(playerData.characterData.characterId);
			}
			
			// Get network manager from game instance and initialize inventory
			if (UNetworkManager* NetworkManager = MyGameInstance->GetNetworkManager())
			{
				InventoryManager->Initialize(NetworkManager);
				InventoryManager->SubscribeToNetworkManager();
			}
			
			// Set reference in game instance for easy access
			MyGameInstance->SetInventoryManager(InventoryManager);

			// Link EquipmentManager so it can sync is_equipped flags on EQUIPMENT_STATE / EQUIP_RESULT
			if (UEquipmentManager* EquipMgr = MyGameInstance->GetEquipmentManager())
			{
				EquipMgr->SetInventoryManager(InventoryManager);
			}
		}

		// Initialize harvest manager
		if (UHarvestManager* HarvestManager = MyGameInstance->GetHarvestManager())
		{
			HarvestManager->SetWorldContext(GetWorld());
			HarvestManager->SetGameInstance(MyGameInstance);

			// Get network manager from game instance and initialize harvest
			if (UNetworkManager* NetworkManager = MyGameInstance->GetNetworkManager())
			{
				HarvestManager->Initialize(NetworkManager);
				HarvestManager->SubscribeToNetworkManager();
			}

			UE_LOG(LogTemp, Warning, TEXT("HarvestManager initialized for local player"));
		}

		// Initialize UI manager with slight delay to ensure everything is ready
		if (UIManager)
		{
			GetWorld()->GetTimerManager().SetTimer(UIInitTimerHandle, [this]()
			{
				// Guard: skip UI initialization for other-client (remote) players.
				// isOtherClient is false at BeginPlay time but may be set to true before
				// the timer fires when this actor represents another connected client.
				if (playerData.isOtherClient)
				{
					return;
				}

				if (InventoryManager)
				{
					// Get PlayerController
					APlayerController* PC = Cast<APlayerController>(GetController());
					
					// Get HarvestManager from GameInstance
					UHarvestManager* HarvestManager = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;

				//get ExperienceManager from GameInstance
					UExperienceManager* ExperienceManager = MyGameInstance ? MyGameInstance->GetExperienceManager() : nullptr;

					// Subscribe to level-up events so we can play LevelUpSound
					if (ExperienceManager && !playerData.isOtherClient)
					{
						ExperienceManager->OnLevelUp.AddDynamic(this, &ABasicPlayer::HandleLevelUp);
					}

					// Subscribe to effectTick to show floating heal/regen numbers above this player.
					// Applied for ALL players (both local and others) so HoT ticks on remote
					// characters also show a number floating over their heads.
					if (MyGameInstance && MyGameInstance->PlayerStatsNetworkHandler)
					{
						MyGameInstance->PlayerStatsNetworkHandler->OnEffectTick.AddDynamic(
							this, &ABasicPlayer::HandleEffectTickFCT);
					}
					
					// Get SkillManager from GameInstance
                    UPlayerSkillManager* SkillManager = MyGameInstance ? MyGameInstance->GetPlayerSkillManager() : nullptr;

                    // Auto-attack: when the server confirms a skill initiation, schedule
                    // the next swing based on the server-assigned cooldown (not animation).
                    if (SkillManager)
                    {
                        SkillManager->OnSkillCastConfirmed.AddDynamic(this, &ABasicPlayer::HandleSkillInitiatedForAutoAttack);
                    }
					
					// Pass input action references for data-driven pre-input handling
					// Must happen BEFORE Initialize so the listener never sees a null action.
					UIManager->SetAltCursorAction(AltCursorAction);

					// Register the Slate pre-input listener for Alt key (data-driven via QueryKeysMappedToAction).
					UIManager->RegisterPreInputListener();

					// Initialize UIManager with all managers
					UIManager->Initialize(InventoryManager, HarvestManager, ExperienceManager, SkillManager);
					
					// Set PlayerController reference immediately after Initialize
					if (PC)
					{
						UIManager->SetPlayerController(PC);
						UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerController set during initialization"));

						UIManager->InitFTCManager(PC);
						UE_LOG(LogTemp, Warning, TEXT("UIManager: Init called for FCT system"));

                        PlayerHUD = UIManager->GetPlayerInterfaceWidget()->GetPlayerHUD();

						// Flush any stats that arrived before the HUD was ready.
						RefreshHUD();

						// Subscribe to StatsManager so effectTick and other paths that
						// bypass ProcessStatsUpdate still update playerData and the HUD.
						if (UPlayerStatsManager* StatsMgr = MyGameInstance ? MyGameInstance->GetPlayerStatsManager() : nullptr)
						{
							StatsMgr->OnStatsUpdated.AddDynamic(this, &ABasicPlayer::HandleStatsManagerUpdate);

							// stats_update packets arrive before UIManager is ready (boundListeners=0 at
							// broadcast time). Replay the cached stats now so AEW shows effects immediately.
							const FPlayerStatsUpdateStruct& Cached = StatsMgr->GetCachedStats();
							if (Cached.characterId > 0)
							{
								HandleStatsManagerUpdate(Cached);
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to get PlayerController during initialization"));
					}
					
					UE_LOG(LogTemp, Warning, TEXT("UIManager initialized with all managers including SkillManager"));

					// Initialize experience widget for this character.
					// ExperienceManager already has correct progression data
					// (including expForCurrentLevel) set by SpawnPlayerForClient
					// before BeginPlay fires.
					if (ExperienceManager && playerData.characterData.characterId > 0)
					{
						UIManager->InitializeExperienceWidget(playerData.characterData.characterId);

						UE_LOG(LogTemp, Warning, TEXT("ExperienceWidget initialized for character %d"), 
							playerData.characterData.characterId);
					}

					// Initialize skill widgets if SkillManager is available
					if (SkillManager)
					{
						UIManager->InitializeSkillWidgets();
						UE_LOG(LogTemp, Warning, TEXT("Skill widgets initialized"));
					}

				// Bind dialogue and quest widgets to their managers
					UDialogueManager* DlgMgr   = MyGameInstance ? MyGameInstance->GetDialogueManager()  : nullptr;
					UQuestManager*    QuestMgr  = MyGameInstance ? MyGameInstance->GetQuestManager()     : nullptr;
					UIManager->InitializeDialogueAndQuestWidgets(DlgMgr, QuestMgr);

				// Bind equipment / vendor / repair / trade widgets to their managers
					UIManager->InitializeItemSystemWidgets(
						MyGameInstance ? MyGameInstance->GetEquipmentManager() : nullptr,
						MyGameInstance ? MyGameInstance->GetVendorManager()    : nullptr,
						MyGameInstance ? MyGameInstance->GetRepairManager()    : nullptr,
						MyGameInstance ? MyGameInstance->GetTradeManager()     : nullptr);

			// Bind skill shop widget to the skill shop manager
				UIManager->InitializeSkillShopWidget(
					MyGameInstance ? MyGameInstance->GetSkillShopManager() : nullptr);

			// Bind character stats widget to the stats manager
				UIManager->InitializeStatsWidget(
					MyGameInstance ? MyGameInstance->GetPlayerStatsManager() : nullptr);

				// Bind progression managers (title + mastery) and character name to stats widget
				if (UPlayerStatsWidget* SW = UIManager->GetPlayerStatsWidget())
				{
					SW->BindToProgressionManagers(
						MyGameInstance ? MyGameInstance->GetTitleManager()   : nullptr,
						MyGameInstance ? MyGameInstance->GetMasteryManager() : nullptr);
					SW->SetCharacterName(playerData.characterData.characterName);
				}

				// Bind titles window
				UIManager->InitializeTitlesWidget(
					MyGameInstance ? MyGameInstance->GetTitleManager()          : nullptr,
					MyGameInstance ? MyGameInstance->GetTitleNetworkHandler()   : nullptr,
					playerData.characterData.characterId);

				// Bind reputation window
				UIManager->InitializeReputationWidget(
					MyGameInstance ? MyGameInstance->GetReputationManager() : nullptr);

				// Bind emote list window
				UIManager->InitializeEmoteListWidget(
					MyGameInstance ? MyGameInstance->GetEmoteManager()        : nullptr,
					MyGameInstance ? MyGameInstance->GetEmoteNetworkHandler() : nullptr,
					playerData.characterData.characterId);

				// Initialize world notification system (bestiary + toast/zone/etc.)
				if (MyGameInstance && MyGameInstance->GetBestiaryNetworkHandler())
					{
						UIManager->InitializeNotificationSystem(MyGameInstance->GetBestiaryNetworkHandler());
						UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: WorldNotificationManager initialized"));

						// Request the bestiary overview so the mob list is populated on login.
						// Skipped in DevMode offline (no chunk to ask — avoids an
						// Error for an expected offline state).
						if (playerData.characterData.characterId > 0 && MyGameInstance && !MyGameInstance->DevModeConfig.bEnabled)
						{
							MyGameInstance->GetBestiaryNetworkHandler()->RequestBestiaryOverview(
								playerData.characterData.characterId);
							UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Requested bestiary overview for character %d"),
								playerData.characterData.characterId);
						}
					}

				// Initialize WIO (World Interactive Objects) widgets
				if (MyGameInstance && MyGameInstance->GetWorldObjectManager())
				{
					UIManager->InitializeWIOWidgets(MyGameInstance->GetWorldObjectManager());
					UE_LOG(LogTemp, Log, TEXT("BasicPlayer: WIO widgets initialized"));
				}

			// Subscribe to weight status and initialize equipment visuals
				if (UEquipmentManager* EqMgr = MyGameInstance ? MyGameInstance->GetEquipmentManager() : nullptr)
				{
					EqMgr->OnWeightStatusChangedDelegate.AddDynamic(this, &ABasicPlayer::HandleWeightStatusChanged);
					UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Subscribed to weight status events"));

					// Initialize equipment visual component so equipped items appear on the mesh
					if (EquipmentVisualComponent)
					{
						UItemManager* ItemMgr = MyGameInstance->GetItemManager();
						EquipmentVisualComponent->Initialize(EqMgr, ItemMgr);
						UE_LOG(LogTemp, Log, TEXT("BasicPlayer: EquipmentVisualComponent initialized"));
					}

					// Bind cosmetic component to live equipment manager so visibility
					// (hair hidden by helmet, etc.) updates automatically on equip/unequip.
					if (CosmeticVisualComponent)
					{
						CosmeticVisualComponent->BindEquipmentManager(EqMgr);
						UE_LOG(LogTemp, Log, TEXT("BasicPlayer: CosmeticVisualComponent bound to EquipmentManager"));
					}

					// Explicitly request equipment state from server.
					// EQUIPMENT_STATE may arrive before the visual component binds its delegate,
					// but Initialize() above already replays the cached state.
					// This request ensures we get up-to-date data even if the server
					// does not push EQUIPMENT_STATE automatically on join.
					if (playerData.characterData.characterId > 0)
					{
						EqMgr->RequestGetEquipment(playerData.characterData.characterId);
						UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Requested equipment state for character %d"),
							playerData.characterData.characterId);
					}
				}

				// Subscribe to ChatManager: routes each received message to the sender's nameplate bubble.
				if (UChatManager* ChatMgr = MyGameInstance ? MyGameInstance->GetChatManager() : nullptr)
				{
					ChatMgr->OnChatMessageReceived.AddDynamic(this, &ABasicPlayer::HandleChatMessageForBubble);
				}

				// For the LOCAL player: when they equip/remove a title, push the new display name
				// to their nameplate so other viewers see it immediately.
				if (!playerData.isOtherClient)
				{
					if (UTitleManager* TitleMgr = MyGameInstance ? MyGameInstance->GetTitleManager() : nullptr)
					{
						TitleMgr->OnTitlesUpdated.AddDynamic(this, &ABasicPlayer::HandleTitlesUpdated);
					}
				}

				// The death screen will be shown automatically via the pending mechanism
				// in UIManager if the character spawned as dead (SetDead_Implementation
				// already called in SpawnPlayerForClient before this timer fires).
				}

				// Subscribe to UIManager's deferred ready signal.
				// OnUIManagerInitialized is broadcast one game-thread tick AFTER
				// PlayerInterfaceWidget->AddToViewport(), which guarantees the
				// render thread has already received the widget before we signal
				// the loading-screen countdown to start.
				UIManager->OnUIManagerInitialized.AddDynamic(this, &ABasicPlayer::HandleUIManagerInitialized);
				UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] UIInitTimer: subscribed to OnUIManagerInitialized"));

				// Fallback: if the Slate NativeTick broadcast never arrives (window
				// throttled while the loading screen is up), drive the gate directly.
				// HandleUIManagerInitialized is once-guarded, so a race is harmless.
				GetWorld()->GetTimerManager().SetTimer(UIInitFallbackTimerHandle, [this]()
				{
					if (playerData.isOtherClient || bUIInitDone) { return; }
					UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] UIInitFallback: NativeTick signal missing — firing HandleUIManagerInitialized directly"));
					HandleUIManagerInitialized();
				}, 3.0f, false);
                
			}, 0.5f, false);
		}
	}
}
