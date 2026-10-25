// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "Prototyping.h"
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

void ABasicPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EnhancedInputComponent && InputMappingContext)
    {
        UE_LOG(LogGameplay, Warning, TEXT("EnhancedInputComponent init"));
        // Get the player controller
        APlayerController* PC = Cast<APlayerController>(GetController());

        // Get the local player subsystem
        UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        // Clear out existing mapping, and add our mapping
        Subsystem->ClearAllMappings();
        Subsystem->AddMappingContext(InputMappingContext, 1);
 
		// Bind Enhanced Input actions
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABasicPlayer::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABasicPlayer::Look);

        // WoW-style mouse button bindings
        if (RightMouseAction)
        {
            EnhancedInputComponent->BindAction(RightMouseAction, ETriggerEvent::Started,   this, &ABasicPlayer::OnRightMousePressed);
            EnhancedInputComponent->BindAction(RightMouseAction, ETriggerEvent::Completed, this, &ABasicPlayer::OnRightMouseReleased);
        }
        if (LeftMouseAction)
        {
            EnhancedInputComponent->BindAction(LeftMouseAction, ETriggerEvent::Started,   this, &ABasicPlayer::OnLeftMousePressed);
            EnhancedInputComponent->BindAction(LeftMouseAction, ETriggerEvent::Completed, this, &ABasicPlayer::OnLeftMouseReleased);
        }

        // Mouse wheel zoom
        if (ScrollAction)
        {
            EnhancedInputComponent->BindAction(ScrollAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnScroll);
        }

        // Alt-cursor toggle.  In Editor/Development builds the Slate pre-input
        // listener catches Alt first; in Shipping (where that API is unavailable)
        // this Enhanced Input binding handles it directly.  The GFrameCounter
        // guard in ToggleAltCursor prevents double-fire when both paths fire.
        if (AltCursorAction)
        {
            EnhancedInputComponent->BindAction(AltCursorAction, ETriggerEvent::Started, this, &ABasicPlayer::OnAltCursorToggle);
        }

        // bind start movement simulation
        EnhancedInputComponent->BindAction(StartMovementSimulationAction, ETriggerEvent::Triggered, this, &ABasicPlayer::StartMovementSimulation);
        // bind stop movement simulation
        EnhancedInputComponent->BindAction(StopMovementSimulationAction, ETriggerEvent::Triggered, this, &ABasicPlayer::StopMovementSimulation);
    
    
        if (AttackAction)
        {
            // Bind attack action (you'll need to add AttackAction to your header file)
            EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnAttackInput);
        }

        // Tab-target: cycle to nearest MOB in forward cone
        if (TabTargetAction)
        {
            EnhancedInputComponent->BindAction(TabTargetAction, ETriggerEvent::Started, this, &ABasicPlayer::OnTabTargetInput);
        }

        // Target self (F1 by default in IMC)
        if (TargetSelfAction)
        {
            EnhancedInputComponent->BindAction(TargetSelfAction, ETriggerEvent::Started, this, &ABasicPlayer::OnTargetSelfInput);
        }

        // Inventory action
        if (InventoryAction)
        {
            EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnInventoryToggle);
        }


        // Skills panel action
        if (SkillsPanelAction)
        {
            EnhancedInputComponent->BindAction(SkillsPanelAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkillsPanelToggle);
        }

        // Individual skill actions
        if (Skill1Action)
        {
            EnhancedInputComponent->BindAction(Skill1Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill1Input);
        }

        if (Skill2Action)
        {
            EnhancedInputComponent->BindAction(Skill2Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill2Input);
        }

        if (Skill3Action)
        {
            EnhancedInputComponent->BindAction(Skill3Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill3Input);
        }

        if (Skill4Action)
        {
            EnhancedInputComponent->BindAction(Skill4Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill4Input);
        }

        if (Skill5Action)
        {
            EnhancedInputComponent->BindAction(Skill5Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill5Input);
        }

        // Interact with NPC
        if (InteractAction)
        {
            EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ABasicPlayer::OnInteractInput);
            UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: InteractAction bound successfully"));
        }
        else
        {
            UE_LOG(LogGameplay, Error, TEXT("BasicPlayer: InteractAction is NULL - assign it in the player Blueprint (Details > Input > InteractAction)"));
        }

        // Quest Journal toggle
        if (QuestJournalAction)
        {
            EnhancedInputComponent->BindAction(QuestJournalAction, ETriggerEvent::Started, this, &ABasicPlayer::OnQuestJournalToggle);
        }

        // Equipment toggle
        if (EquipmentAction)
        {
            EnhancedInputComponent->BindAction(EquipmentAction, ETriggerEvent::Started, this, &ABasicPlayer::OnEquipmentToggle);
        }

        // Character stats window toggle
        if (StatsAction)
        {
            EnhancedInputComponent->BindAction(StatsAction, ETriggerEvent::Started, this, &ABasicPlayer::OnStatsToggle);
        }

        // Bestiary window toggle
        if (BestiaryAction)
        {
            EnhancedInputComponent->BindAction(BestiaryAction, ETriggerEvent::Started, this, &ABasicPlayer::OnBestiaryToggle);
        }

        // Titles window toggle
        if (TitlesAction)
        {
            EnhancedInputComponent->BindAction(TitlesAction, ETriggerEvent::Started, this, &ABasicPlayer::OnTitlesToggle);
        }

        // Reputation window toggle
        if (ReputationAction)
        {
            EnhancedInputComponent->BindAction(ReputationAction, ETriggerEvent::Started, this, &ABasicPlayer::OnReputationToggle);
        }

        // Emote list toggle
        if (EmoteListAction)
        {
            EnhancedInputComponent->BindAction(EmoteListAction, ETriggerEvent::Started, this, &ABasicPlayer::OnEmoteListToggle);
        }

		// Main game menu toggle (Escape)
		if (GameMenuAction)
		{
			EnhancedInputComponent->BindAction(GameMenuAction, ETriggerEvent::Started, this, &ABasicPlayer::OnGameMenuToggle);
		}

		// Jump (Space)
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABasicPlayer::OnJumpPressed);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABasicPlayer::OnJumpReleased);
		}
	}
}

// Sets default values
ABasicPlayer::ABasicPlayer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    bUseControllerRotationYaw = false;

	//Create audio component
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);

    // Create spring arm пїЅ controls camera distance and pitch
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength       = DesiredZoom;  // initial distance
    CameraBoom->bUsePawnControlRotation = true;        // arm rotates with controller
    CameraBoom->bEnableCameraLag       = true;
    CameraBoom->CameraLagSpeed         = 8.0f;
    CameraBoom->bEnableCameraRotationLag = false;
    // Only collide with world geometry (walls, terrain) вЂ” ignore pawns (mobs, NPCs, players)
    // so the spring arm does not compress when another character stands between the camera and the player.
    // Use ECC_Camera (not ECC_Visibility) because ECC_Visibility is also used for line-of-sight
    // checks вЂ” mobs/NPCs must still block Visibility for targeting, but must NOT block Camera.
    CameraBoom->ProbeChannel           = ECC_Camera;

    // Prevent other players' capsules from compressing the spring arm.
    // The owning pawn is already auto-ignored by the spring arm probe.
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }

    // Create the follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false; // camera does not rotate пїЅ arm does

	// Create UI Manager component
	UIManager = CreateDefaultSubobject<UUIManager>(TEXT("UIManager"));

	// Create Inventory Manager component  
	InventoryManager = CreateDefaultSubobject<UInventoryManager>(TEXT("InventoryManager"));

	// Create nameplate component пїЅ auto-hidden for local player, visible for remote players
	NameplateComponent = CreateDefaultSubobject<UPlayerNameplateComponent>(TEXT("NameplateComponent"));

	// Create equipment visual component — attaches item meshes to skeleton sockets
	EquipmentVisualComponent = CreateDefaultSubobject<UEquipmentVisualComponent>(TEXT("EquipmentVisualComponent"));

	// Create cosmetic visual component — manages hair, facial hair, etc. via Leader Pose
	CosmeticVisualComponent = CreateDefaultSubobject<UCosmeticVisualComponent>(TEXT("CosmeticVisualComponent"));

	// Create emote component - handles emote montage playback, VFX, audio, and interruption
	EmoteComponent = CreateDefaultSubobject<UEmoteComponent>(TEXT("EmoteComponent"));

	// Create cursor interaction component - hover trace, click/double-click, cursor icons, decal management
	CursorInteractionComponent = CreateDefaultSubobject<UCursorInteractionComponent>(TEXT("CursorInteractionComponent"));

	// Create floor-circle decal for when THIS player is targeted by other players
	TargetDecal = CreateDefaultSubobject<UTargetDecalComponent>(TEXT("TargetDecal"));
	TargetDecal->SetupAttachment(RootComponent);

    // Init simulation variables
    SquareCenter = FVector(0.f, 0.f, 90.f); // Assuming Z is up and you want to move around this center
    SideLength = 1000.f; // The length of the side of the square
    TargetPosition = SquareCenter + FVector(SideLength / 2, 0.f, 0.f); // Start with a target position

    // Sync CharacterMovementComponent speed with MoveSpeed so AddMovementInput(dir, 1.0)
    // produces a frame-rate-independent velocity matching the server attribute move_speed.
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->MaxWalkSpeed = MoveSpeed;
        CMC->bOrientRotationToMovement = false;
        CMC->bUseControllerDesiredRotation = false;
        CMC->JumpZVelocity = 420.0f;
        CMC->AirControl = 0.35f;
    }
    JumpMaxHoldTime = 0.2f;
}

// Called when the game starts or when spawned
