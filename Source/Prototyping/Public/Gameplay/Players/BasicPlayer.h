// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "InputMappingContext.h"
#include "Components/AudioComponent.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "Animation/AnimNotify_PlayerCombatEvent.h"
#include "BasicPlayer.generated.h"

class UMyGameInstance;
class ABasicMOB;
class ABasicNPC;

// Event declaration
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZoneUpdated, int32, PlayerID);

UCLASS()
class PROTOTYPING_API ABasicPlayer : public ACharacter, public ICombatable, public IWorldInteractable
{
	GENERATED_BODY()

private:
	// player data
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FClientDataStruct playerData;
	// message data
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FMessageDataStruct messageData;

	int32 LastPreUpdateHP = -1;
	int32 LastPreUpdateMP = -1;

	UPROPERTY()
	UMyGameInstance* MyGameInstance;

	// Camera components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

	// Zoom
	float DesiredZoom = 600.0f;
	float ZoomStep = 30.0f;
	float ZoomMin = 200.0f;
	float ZoomMax = 1200.0f;
	float ZoomInterpSpeed = 8.0f;

	// WoW-like camera tuning
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraPitchMin = -80.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraPitchMax = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraPitchSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraYawSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	bool bEnableMouseButtonsMoveForward = true;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MouseButtonsMoveForwardScale = 1.0f;

	// Mouse button state
	bool bIsRightMouseDown = false;
	bool bIsLeftMouseDown = false;

	// Mesh rotation speed (degrees per second for keyboard turning)
	float MeshRotationSpeed = 180.0f;

	// Remote player animation data
	float RemoteSpeed = 0.0f;
	float RemoteDirection = 0.0f;

	// EMA-smoothed versions fed to AnimInstance so transitions are gradual.
	float SmoothedRemoteSpeed = 0.0f;
	float SmoothedRemoteDirection = 0.0f;

	// True while the server is actively sending position updates for this remote player.
	bool bRemoteIsMoving = false;
	// Accumulates time since the last packet that contained actual displacement.
	float RemoteIdleTime = 0.0f;

	// True when the remote player is in the air (jumping/falling), driven by isFalling from server.
	bool bRemoteIsInAir = false;

	// When true, UpdateRemotePlayerMovement is skipped so CharacterPreviewManager
	// can drive position and animation directly during character-select podium walks.
	bool bPreviewMovementActive = false;

	// Server move_speed -> Unreal units conversion scale
	// Server validates: maxAllowedDist = move_speed * 40.0 * dt * 1.3 (30% buffer)
	// Must match exactly so the client speed == server expectation.
	float MoveSpeedScale = 40.0f;

	// NPC interaction tracking
	UPROPERTY()
	ABasicNPC* TrackedNPC = nullptr;

	// WIO interaction tracking
	UPROPERTY()
	class AWorldInteractiveObjectActor* TrackedWIOActor = nullptr;

	/** Delegate handle for the currently tracked WIO actor proximity. */
	FDelegateHandle WIOProximityDelegateHandle;

	// Animation delegate handles
	FDelegateHandle HitPointDelegateHandle;
	FDelegateHandle AnimEndDelegateHandle;
	FTimerHandle HitPointTimerHandle;

	// Gameplay-side cast lock timer: fires HideCastBar_Implementation after cast finishes
	FTimerHandle CastBarTimerHandle;

	// Timer for deferred UI initialization in BeginPlay
	FTimerHandle UIInitTimerHandle;

	// True once NotifyPlayerSpawned has been sent to GameInstance so we don't
	// fire it more than once per spawn even if Tick runs before the flag is acked.
	bool bSpawnNotified = false;

	// Set to true by UIInitTimer after all widgets are ready. Tick checks this
	// before firing NotifyPlayerSpawned so the flag is raised only after both
	// the camera has had at least one real Tick and the UI is fully constructed.
	bool bUIInitDone = false;

public:
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	/**
	 * Row key in DT_EntityAudioProfiles used to drive all audio for this player character.
	 * Assign a row name (e.g. "warrior_m", "mage_f", "archer_m") in the player Blueprint.
	 * All individual sound slots (death, revive, hit, heal, voice, etc.) are read from
	 * the matching FEntityAudioProfile row at runtime.  Leave as NAME_None to play silence
	 * on all generic events (skill-specific sounds still work via SkillDefinition table).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
	FName AudioProfileId = FName("warrior_m");

	/** Returns the audio profile for this player from the GameInstance repository. 
	 *  Returns nullptr when AudioProfileId is not set or when the repository is not ready. */
	const FEntityAudioProfile* GetAudioProfile() const;

	// Nameplate component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UPlayerNameplateComponent* NameplateComponent;

	// ── Chat Bubble ──────────────────────────────────────────────────────────

	/** How many seconds the chat speech bubble stays visible above the sender's nameplate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat Bubble", meta = (AllowPrivateAccess = "true"))
	float ChatBubbleDisplayDuration = 5.0f;

	// Equipment visual component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	class UEquipmentVisualComponent* EquipmentVisualComponent;

	// Cosmetic visual component — manages hair, facial hair, etc. using Leader Pose
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	class UCosmeticVisualComponent* CosmeticVisualComponent;

	// Emote component — handles montage playback, VFX, and interruption for emotes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Emotes", meta = (AllowPrivateAccess = "true"))
	class UEmoteComponent* EmoteComponent;

	// ── Cursor Interaction ────────────────────────────────────────────────────

	/** Handles hover trace, click/double-click detection, cursor icons, and decal states. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Interaction",
		meta = (AllowPrivateAccess = "true"))
	class UCursorInteractionComponent* CursorInteractionComponent;

	/** Floor-circle decal for targeting this player (remote player selection). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Interaction",
		meta = (AllowPrivateAccess = "true"))
	class UTargetDecalComponent* TargetDecal;

	// Pickup lock
	bool bIsPickingUp = false;

	// Cast lock: true while a skill with cast time is in progress
	bool bIsCasting = false;

	// Auto-attack animation delegate handle
	FDelegateHandle AutoAttackAnimEndDelegateHandle;

	// Auto-attack swing delay
	float AutoAttackSwingDelay = 0.5f;

	// Attack range (Unreal units)
	float AttackRange = 300.0f;

	// Tolerance for server range comparison
	float AttackRangeServerTolerance = 50.0f;
	
	// Zone Name
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FString CurrentZoneName;

	// Combat system target tracking
	int32 CurrentTargetId = 0;
	ECasterType CurrentTargetType = ECasterType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	// Skill slug used by the auto-attack loop. Always "basic_attack" unless
	// explicitly changed. Must NOT be overwritten by PlaySkillAnimation.
	FString CurrentSkillName = "basic_attack";

	// Skill slug of the currently-playing animation (set by PlaySkillAnimation_Implementation).
	// Used ONLY for audio/VFX lookups inside PlayCombatSoundEvent.
	// Kept separate from CurrentSkillName so the auto-attack loop always sends basic_attack.
	FString ActiveAnimSkillSlug = TEXT("basic_attack");

	// Previous soft-highlighted target
	UPROPERTY()
	ABasicMOB* PrevSoftTarget = nullptr;

	// Approach movement state
	bool bIsApproachingTarget = false;
	FString PendingSkillSlug;

	// ── Cursor Interaction: pending non-combat approach ──────────────────────
	// When a cursor double-click targets an NPC/Item/Harvest out of range,
	// the player walks toward PendingInteractionTarget and calls
	// DispatchPendingInteraction() on arrival.

	enum class EPendingInteraction : uint8
	{
		None,
		AutoAttack,
		Harvest,
		TalkNPC,
		PickupItem,
	};

	EPendingInteraction PendingInteraction = EPendingInteraction::None;
	TWeakObjectPtr<AActor> PendingInteractionTarget;

	// ── LMB click / drag state ───────────────────────────────────────────────
	// OnLeftMousePressed records press time and position.
	// Look() accumulates pixel movement; once it exceeds the drag threshold
	// it sets bLMBDragActive and captures the mouse.
	// OnLeftMouseReleased fires HandleConfirmedClick() if NOT dragging.

	float     LMBPressTime   = 0.f;
	float     LMBDragPixelsAccum = 0.f;
	bool      bLMBDragActive = false;

	// Accumulates DeltaTime for the throttled NPC-interaction distance check in Tick.
	float     NpcDistCheckAccum = 0.f;

	// Cached from the most recent combatInitiation packet.
	// Used at CastRelease to compute distance-proportional projectile speed.
	float CurrentAnimationDuration = 0.0f; // animationDuration from server
	float CurrentCastTime          = 0.0f; // castTime from server (cast bar duration)
	// Pre-computed at PlaySkillAnimation time so HideCastBar reset of CurrentCastTime
	// cannot corrupt the projectile speed calculation at CastRelease notify time.
	float CurrentSwingSeconds      = 0.0f; // = animDuration - castTime (or animDuration for instant)
	FTimerHandle AutoAttackRetryTimerHandle;

	// Mesh rotation desire
	float DesiredMeshYaw = 0.f;
	bool bHasDesiredMeshYaw = false;


	// set editable variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float interpolationSpeedFactor = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float maxInterpolationSpeed = 1200.0f;

	float TimeSinceLastUpdate = 0.0f;
	float LastUpdateTime = 0.0f;

	FVector LastSentPosition;
	FRotator LastSentRotation;

	// rotation threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float RotationThreshold = 5.0f;
	// distance threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float PositionThreshold = 5.0f;
	// movement packet send update interval
	const float UpdateInterval = 0.1f;

	// Constant movement speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float MoveSpeed = 200.0f; // Adjust as needed

	// Constant rotation speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float RotationSpeed = 1000.0f; // Adjust as needed

	// Variables for square movement
	FVector SquareCenter;
	float SideLength;
	FVector TargetPosition;

	// Control simulation
	bool bSimulateMovement;


	/** ����� HUD, ������� ����� ����������� */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/** ������ �� ��������� HUD */
	UPROPERTY()
	UPlayerHUD* PlayerHUD;

	void AttackActor(AActor* TargetActor, const FString& SkillSlug);

	class UPlayerAnimInstance* GetPlayerAnimInstance() const;

	UFUNCTION()
	void OnAttackInput();

	UFUNCTION()
	void OnJumpPressed();

	UFUNCTION()
	void OnJumpReleased();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when the actor is being destroyed
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Add this input action (you'll need to set it up in Blueprint)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* AttackAction;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Sets default values for this character's properties
	ABasicPlayer();

	void UpdateHUD();

	/**
	 * Called by AnimNotify_PlayerCombatEvent to fire audio and VFX timed to the montage.
	 * SwingSound  – weapon whoosh: equipped-weapon swing → SkillDefinition.swingSound.
	 * VoiceAttack – random grunt from the EntityAudioProfile VoiceAttack pool (local player only).
	 * CastRelease – release sound + Niagara VFX from castEndSound / castEndEffectNiagara.
	 */
	UFUNCTION()
	void PlayCombatSoundEvent(ECombatSoundSlot Slot);

	// ICombatable interface implementation
	virtual int32 GetActorId_Implementation() const override 
	{ 
		int32 CharacterId = playerData.characterData.characterId;
		return CharacterId > 0 ? CharacterId : 0;
	}
	virtual ECasterType GetActorType_Implementation() const override { return ECasterType::Player; }
	virtual FString GetActorTypeString_Implementation() const override { return TEXT("Player"); }
	
	virtual int32 GetCurrentHealth_Implementation() const override { return playerData.characterData.characterCurrentHealth; }
	virtual int32 GetMaxHealth_Implementation() const override;
	virtual int32 GetCurrentMana_Implementation() const override { return playerData.characterData.characterCurrentMana; }
	virtual int32 GetMaxMana_Implementation() const override;
	
	virtual void SetCurrentHealth_Implementation(int32 NewHealth) override { SetPlayerCurrentHPPoints(NewHealth); }
	virtual void SetCurrentMana_Implementation(int32 NewMana) override { SetPlayerCurrentMPPoints(NewMana); }
	
	virtual bool IsDead_Implementation() const override { return GetIsDead(); }
	virtual void SetDead_Implementation(bool bNewDead) override;
	virtual void OnDeath_Implementation() override;
	
	virtual FVector GetCombatPosition_Implementation() const override { return GetActorLocation() + FVector(0, 0, 120); }
	
	virtual void SetTarget_Implementation(int32 TargetId, ECasterType TargetType) override;
	virtual void ClearTarget_Implementation() override;
	
	virtual void PlaySkillAnimation_Implementation(const FString& AnimationName, const FString& SkillSlug, float Duration = 0.0f) override;
	virtual void ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School, bool bIsMissed, bool bIsBlocked, const FString& SkillSlug) override;
	virtual void ShowHealingEffect_Implementation(int32 Healing, const FString& SkillSlug) override;
	virtual void ShowManaRestoreEffect_Implementation(int32 ManaRestored) override;
	virtual void ShowBuffEffect_Implementation(const FAppliedEffectData& Effect) override;
	virtual void ShowCastBar_Implementation(float CastTime, const FString& SkillName) override;
	virtual void HideCastBar_Implementation() override;

	// IWorldInteractable interface (used when another player clicks on this remote player)
	virtual EInteractableType GetInteractableType()    const override;
	virtual FText GetInteractableDisplayName()         const override;
	virtual bool  CanInteract()                        const override { return playerData.isOtherClient; }

	// Event variable
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnZoneUpdated ZoneUpdated;

	// Battle system functions
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool AttackTarget(int32 TargetID, const FString& SkillSlug, int32 TargetTypeId = 3);

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Enhanced Input movement functions for the current player
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	// Convert string to timestamp
	FDateTime StringToTimestamp(const FString& DateTimeString);

	//SIMULATION OF MOVEMENT FUNCTIONS
	void StartMovementSimulation();
	void StopMovementSimulation();
	void UpdateMovementSimulation(float DeltaTime);

	// Update LOCAL player movement
	void UpdateCurrentPlayerMovement(float DeltaTime);

	// Update REMOTE player movement
	void UpdateRemotePlayerMovement();
	float CalculateRotationInterpSpeed();
	// Interpolate movement for REMOTE player
	float CalculateInterpolationSpeed(float MovementSpeed);

	// Set Is Other Client
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetIsOtherClient(bool bIsOtherClient);

	// Set client ID
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetClientID(int32 ID);

	void SetPlayerTag(const FString& Tag);

	// Set client token
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetClientSecret(FString Secret);

	// Set character ID
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetCharacterID(int32 ID);

	// Set client login
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetClientLogin(FString Login);

	// set player coordinates
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetCoordinates(double x, double y, double z, double rotZ, bool bIsFalling = false);

	// set player class
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerClass(FString Class);

	// set player race
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerRace(FString Race);

	// set player gender
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerGender(FString Gender);

	// set player name
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerName(FString Name);

	// set player level
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerLevel(int32 Level);

	// set player experience points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerExpPoints(int32 ExpPoints);

	//set player next level exp
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerNextLevelExp(int32 NextLevelExp);

	// Update experience data in ExperienceManager
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void UpdateExperienceData();

	// set player current HP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerCurrentHPPoints(int32 CurrentHPPoints);

	// set player current MP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerCurrentMPPoints(int32 CurrentMPPoints);

	//set player attributes
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerAttributes(TMap<FString, FAttributeDataStruct> Attributes);

	// Apply server-authoritative move_speed to CharacterMovementComponent.
	// ServerMoveSpeed is the raw server value; it is multiplied by MoveSpeedScale internally.
	// This is the single source of truth for MaxWalkSpeed — no client-side modifiers.
	void ApplyServerMoveSpeed(float ServerMoveSpeed);

	/**
	 * Look up this character's class+race+gender in the CharacterVisualDefinitionsTable
	 * and async-load the correct SkeletalMesh + AnimBP. Follows the same pattern as
	 * BasicMOB::SetupMobVisual.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player Visual")
	void ApplyVisualFromDataTable(UDataTable* VisualTable);

	// Set message data
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetMessageData(const FMessageDataStruct NewMessageData);

	// Set zone name
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetCurrentZoneName(const FString& NewZoneName);

	// Get is other client
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool GetIsOtherClient();
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool GetIsDead() const;
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool GetIsMoving() const;

	UFUNCTION(BlueprintCallable, Category = "Player Data")
	FString GetCurrentZoneName();

	// get player current HP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	int32 GetPlayerCurrentHPPoints() const;

	// get player current MP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	int32 GetPlayerCurrentMPPoints() const;

	// get player id
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	int32 GetPlayerID() const { return playerData.clientId; }

	FVector LastFrameLocation;
	

	// input mapping context and actions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* InputMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	// start movement simulation action
	UInputAction* StartMovementSimulationAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	// stop movement simulation action
	UInputAction* StopMovementSimulationAction;

	// Inventory input action
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* InventoryAction;

	// Harvest input action
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* HarvestAction;

	// Skills input actions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* SkillsPanelAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* Skill1Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* Skill2Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* Skill3Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* Skill4Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* Skill5Action;

	// Reference to the login camera actor
	UPROPERTY()
	ACameraActor* LoginCameraActor;

	// Audio component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAudioComponent* AudioComponent;

	// Play sound
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlaySound(USoundBase* Sound);

	// Stop sound
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StopSound();


	void CheckForMOB();

	void CreateHUD();

	// Remote player interpolation
	FVector LastReceivedPosition;
	FVector TargetReceivedPosition;
	FRotator LastReceivedRotation;
	FRotator TargetReceivedRotation;
	float TimeSinceLastPositionUpdate = 0.0f;
	float ServerPositionUpdateInterval = 0.1f; // server update interval (100 ms)

	// Dead-reckoning velocity (world-space, Unreal units/sec).
	// Recalculated from consecutive server positions every packet.
	// Open-loop integration — drift is bounded by the update interval
	// and reset by each incoming packet.
	FVector RemoteVelocity = FVector::ZeroVector;

	// Gap-correction strength (0-1).  Each packet adds Gap * (strength / interval)
	// to the dead-reckoning velocity, closing actor→target lag in ~0.5 s
	// entirely through speed adjustment — never via SetActorLocation jerking.
	// Negative gap (jitter overshoot) lowers velocity, no back-step.
	// Tune via Blueprint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration",
		meta = (AllowPrivateAccess = "true"))
	float PositionBlendStrength = 0.2f;

	// True after the first real server position packet has been applied.
	// Until then, UpdateRemotePlayerMovement does a hard snap instead of lerp
	// so the actor never slides from world-origin (0,0,0) to the spawn point.
	bool bHasReceivedFirstPosition = false;


	public:
		// Get player character ID
		UFUNCTION(BlueprintCallable, Category = "Player")
		int32 GetPlayerCharacterID() const { return playerData.characterData.characterId; }




			UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
			class UInputAction* PickupAction;


			UFUNCTION(BlueprintCallable, Category = "Player Data")
			FClientDataStruct GetPlayerData() const { return playerData; }

			// Get just the position data from playerData
			UFUNCTION(BlueprintCallable, Category = "Player Data")
			FPositionDataStruct GetPlayerDataPosition() const { return playerData.characterData.characterPosition; }

			// Update player stats from server data (for stats_update event)
			UFUNCTION(BlueprintCallable, Category = "Player Data")
			void UpdatePlayerStats(const FPlayerStatsUpdateStruct& StatsUpdate);

			// Handle stats update and refresh UI
			UFUNCTION(BlueprintCallable, Category = "Player Data")
			void ProcessStatsUpdate(const FPlayerStatsUpdateStruct& StatsUpdate);

			// Force refresh HUD with current player stats
			UFUNCTION(BlueprintCallable, Category = "UI")
			void RefreshHUD();

			UFUNCTION(BlueprintCallable, Category = "UI")
			UUIManager* GetUIManager() const { return UIManager; }

			// Called by PlayerStatsManager::OnStatsUpdated to keep playerData and HUD in sync
			// for all update paths (including effectTick).
			UFUNCTION()
			void HandleStatsManagerUpdate(const FPlayerStatsUpdateStruct& NewStats);

			UFUNCTION()
			void HandleSkillInitiatedForAutoAttack(const FString& SkillSlug, int32 CooldownMs);

		protected:
			UFUNCTION()
			void OnPickupInput();

			// Handle inventory input
	UFUNCTION()
	void OnInventoryToggle();

	// UI Manager component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	class UUIManager* UIManager;

	// Inventory Manager component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	class UInventoryManager* InventoryManager;

	// Handle harvest input
	UFUNCTION()
	void OnHarvestInput();

	// Handle skills input
	UFUNCTION()
	void OnSkillsPanelToggle();

	UFUNCTION()
	void OnSkill1Input();

	UFUNCTION()
	void OnSkill2Input();

	UFUNCTION()
	void OnSkill3Input();

	UFUNCTION()
	void OnSkill4Input();

	UFUNCTION()
	void OnSkill5Input();

	UFUNCTION()
	void OnTargetSelfInput();

public:
	// Target lock system
	UPROPERTY()
	ABasicMOB* LockedTarget = nullptr;

	// Player target lock (for heals/buffs on other players)
	UPROPERTY()
	ABasicPlayer* LockedPlayerTarget = nullptr;

	// Self-target flag — when true, skills cast on self even if another target is visible
	bool bIsSelfTargeted = false;

	bool bIsAutoAttacking = false;
	bool bLastKnownTargetAggro = false;

	/** Returns the nameplate component (valid for both local and remote players). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	UPlayerNameplateComponent* GetNameplateComponent() const { return NameplateComponent; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetLockedTarget(ABasicMOB* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetLockedPlayerTarget(ABasicPlayer* NewTarget);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	ABasicMOB* GetLockedTarget() const { return LockedTarget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	ABasicPlayer* GetLockedPlayerTarget() const { return LockedPlayerTarget; }

	// Returns the currently locked target actor (mob or player), or nullptr
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	AActor* GetLockedTargetActor() const;

	// Returns the locked target's combat ID (characterId for players, mobUId for mobs)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	int32 GetLockedTargetId() const;

	// Returns the locked target's ECasterType
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	ECasterType GetLockedTargetType() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearLockedTarget();

	// Self-target: directs SelfAndTarget/SelfOnly skills to cast on self
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TargetSelf();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	bool IsSelfTargeted() const { return bIsSelfTargeted; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	float GetCurrentSkillRange() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void DoAutoAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FaceLockedTarget();

	// Stop auto-attack but keep target lock
	void StopAutoAttack();

	// Update approach movement towards locked target
	void UpdateApproach(float DeltaTime);

	// Get skill range by slug
	float GetSkillRange(const FString& SkillSlug) const;

	// Try to cast skill with approach if out of range
	void TryCastSkillWithApproach(const FString& SkillSlug);

	// Get skill slot index for a given slug
	int32 GetSkillSlotIndexForSlug(const FString& SkillSlug) const;

	// Tab target cycling
	void OnTabTargetInput();

	// Pickup lock/unlock movement
	void LockMovementForPickup();
	void UnlockMovementAfterPickup();

	// Set the desired mesh yaw for smooth rotation (used e.g. when facing a pickup target)
	void SetDesiredFaceYaw(float Yaw)
	{
		DesiredMeshYaw    = Yaw;
		bHasDesiredMeshYaw = true;
	}

	// Right/Left mouse button handlers
	void OnRightMousePressed();
	void OnRightMouseReleased();
	void OnLeftMousePressed();
	void OnLeftMouseReleased();
	void OnScroll(const FInputActionValue& Value);
	void ApplyMouseCaptureIfNoUIOpen();
	void RestoreCursorToUIManager();
	void UpdateMeshRotation(float DeltaTime);
	void HandleMouseButtonsMoveForward();
	void ClampControlPitch();

	// ── Cursor Interaction Dispatch ───────────────────────────────────────────

	/**
	 * Single-click handler: visually "select" the target without triggering any action.
	 * - MOB alive / dead     → SetLockedTarget / decal lock (no auto-attack)
	 * - NPC                  → visual lock + show nameplate
	 * - DroppedItem          → visual lock
	 * - RemotePlayer         → visual lock
     * - null (empty ground)  → no-op (target is preserved; cleared via Esc or distance only)
	 */
	UFUNCTION()
	void DispatchCursorSelect(AActor* Target, EInteractableType Type);

	/**
	 * Double-click handler: execute the contextual action for the target.
	 * If the target is out of range, starts auto-approach and stores
	 * EPendingInteraction so the action fires on arrival.
	 */
	UFUNCTION()
	void DispatchCursorInteract(AActor* Target, EInteractableType Type);

	/** Called by UpdateApproach when PendingInteraction is set and the player has arrived. */
	void DispatchPendingInteraction();

	/** Helper: returns InteractionRange from CursorInteractionComponent config, or 280 cm. */
	float GetInteractionRange() const;

	/** Returns true when a UI window is open and should consume mouse input exclusively. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction")
	bool IsUIBlockingInteraction() const;

	// Initialise nameplate
	void InitialiseNameplate(bool bIsLocal);

	// Get equipment visual component (for external initialization of remote players)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	class UEquipmentVisualComponent* GetEquipmentVisualComponent() const { return EquipmentVisualComponent; }

	// Get cosmetic visual component (hair/facial hair — initialized via ApplyVisualFromDataTable)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	class UCosmeticVisualComponent* GetCosmeticVisualComponent() const { return CosmeticVisualComponent; }

	// Get emote component
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emotes")
	class UEmoteComponent* GetEmoteComponent() const { return EmoteComponent; }

	/**
	 * Called by the emote routing code when emoteAction arrives for this character's ID.
	 * Forwards to UEmoteComponent::PlayEmoteBySlug().
	 */
	UFUNCTION(BlueprintCallable, Category = "Emotes")
	void PlayEmoteForCharacter(const FString& EmoteSlug, const FString& AnimationName);

	// Play event sound from soft ref
	void PlayEventSound(const TSoftObjectPtr<USoundBase>& SoundRef);

	// Additional input toggles
	UFUNCTION()
	void OnQuestJournalToggle();
	UFUNCTION()
	void OnEquipmentToggle();
	UFUNCTION()
	void OnAltCursorToggle();
	UFUNCTION()
	void OnStatsToggle();
	UFUNCTION()
	void OnBestiaryToggle();
	UFUNCTION()
	void OnTitlesToggle();
	UFUNCTION()
	void OnReputationToggle();
	UFUNCTION()
	void OnEmoteListToggle();
	UFUNCTION()
	void OnGameMenuToggle();
	UFUNCTION()
	void OnInteractInput();

	// WIO (World Interactive Objects)
	UFUNCTION()
	void HandleWIOActorSpawned(class AWorldInteractiveObjectActor* SpawnedActor);
	UFUNCTION()
	void HandleWIOProximityChanged(class AWorldInteractiveObjectActor* WIOActor, bool bInRange);
	void TryInteractWithWIO();
	void CancelWIOChannelIfActive();

	void CheckForNPC();

	// Checks if the player has walked too far from the NPC whose UI windows are open,
	// and force-closes those windows if so. Called from Tick at a throttled interval.
	void CheckNPCInteractionDistance();

	// Death screen
	void ShowDeathScreen();
	void HideDeathScreen();

	// Called when the player is revived
	void OnRevive();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Data")
	float GetRemoteSpeed() const { return SmoothedRemoteSpeed; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Data")
	float GetRemoteDirection() const { return SmoothedRemoteDirection; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Data")
	bool GetRemoteIsInAir() const { return bRemoteIsInAir; }

	// ─── Character-select preview movement (CharacterPreviewManager use only) ────
	/**
	 * Engage preview-movement mode: suppresses UpdateRemotePlayerMovement so
	 * CharacterPreviewManager can drive position and animation directly.
	 */
	void SetPreviewMovementActive(bool bActive) { bPreviewMovementActive = bActive; }
	bool IsPreviewMovementActive() const        { return bPreviewMovementActive; }

	/**
	 * Feed the animation system during a preview walk without a server packet.
	 * Sets SmoothedRemoteSpeed / Direction so the AnimBP blend-space reacts.
	 */
	void SetPreviewAnimationSpeed(float Speed, float Direction);

	/**
	 * Finalise a preview move: sync the received-position state so
	 * UpdateRemotePlayerMovement stays put at FinalPos once re-enabled.
	 */
	void FinishPreviewMovement(const FVector& FinalPos, const FRotator& FinalRot);
	// ─────────────────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnRespawnClicked();

	// Level-up handler
	UFUNCTION()
	void HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience);

	// Weight status handler
	UFUNCTION()
	void HandleWeightStatusChanged(const FWeightStatusData& WeightStatus);

	// Handles effectTick broadcast from PlayerStatsNetworkHandler.
	// Shows floating "+N" numbers above this player when a HoT tick lands.
	UFUNCTION()
	void HandleEffectTickFCT(const FEffectTickData& TickData);

	// Handles incoming chat messages: shows the bubble if the message belongs to this player.
	UFUNCTION()
	void HandleChatMessageForBubble(const FChatMessageStruct& Message);

	// Called when the local player equips/removes a title; routes the display name to the nameplate.
	UFUNCTION()
	void HandleTitlesUpdated(const FPlayerTitlesState& State);

	// Push an equipped title display name to this player's nameplate (called by the server spawn path
	// for remote players once the server includes title data in the player spawn handshake).
	UFUNCTION(BlueprintCallable, Category = "Player|Title")
	void SetEquippedTitle(const FString& TitleDisplayName);

	// Called one tick after PlayerInterfaceWidget->AddToViewport() via UIManager delegate.
	// This is the authoritative signal that the game UI is visible to the renderer.
	UFUNCTION()
	void HandleUIManagerInitialized();

	// Additional input actions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* TabTargetAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* RightMouseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* LeftMouseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ScrollAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* QuestJournalAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* EquipmentAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* AltCursorAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* StatsAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* BestiaryAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* TitlesAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ReputationAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* EmoteListAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* TargetSelfAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* GameMenuAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* JumpAction;
};
