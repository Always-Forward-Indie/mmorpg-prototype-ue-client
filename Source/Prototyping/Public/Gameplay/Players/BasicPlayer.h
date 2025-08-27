// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/AudioComponent.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/Combat/ICombatable.h"
#include "BasicPlayer.generated.h"

class UMyGameInstance;

// Event declaration
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZoneUpdated, int32, PlayerID);

UCLASS()
class PROTOTYPING_API ABasicPlayer : public ACharacter, public ICombatable
{
	GENERATED_BODY()

private:
	// player data
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FClientDataStruct playerData;
	// message data
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FMessageDataStruct messageData;

	UMyGameInstance* MyGameInstance;
	
	// Zone Name
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FString CurrentZoneName;

	// Combat system target tracking
	int32 CurrentTargetId = 0;
	ECasterType CurrentTargetType = ECasterType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	//skill name
	FString CurrentSkillName = "basic_attack";


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


	/** Класс HUD, который будет создаваться */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/** Ссылка на созданный HUD */
	UPROPERTY()
	UPlayerHUD* PlayerHUD;

	void AttackActor(AActor* TargetActor, const FString& SkillSlug);

	UFUNCTION()
	void OnAttackInput();

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
	
	virtual void PlaySkillAnimation_Implementation(const FString& AnimationName, float Duration = 0.0f) override;
	virtual void ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School) override;
	virtual void ShowHealingEffect_Implementation(int32 Healing) override;
	virtual void ShowBuffEffect_Implementation(const FAppliedEffectData& Effect) override;

	// Event variable
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnZoneUpdated ZoneUpdated;

	// Battle system functions
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void AttackTarget(int32 TargetID, const FString& SkillSlug, int32 TargetTypeId = 3);

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
	void SetCoordinates(double x, double y, double z, double rotZ);

	// set player class
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerClass(FString Class);

	// set player race
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerRace(FString Race);

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

	// set player current HP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerCurrentHPPoints(int32 CurrentHPPoints);

	// set player current MP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerCurrentMPPoints(int32 CurrentMPPoints);

	//set player attributes
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerAttributes(TMap<FString, FAttributeDataStruct> Attributes);

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

	// Reference to the login camera actor
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
	float TimeSinceLastPositionUpdate;
	float ServerPositionUpdateInterval = 0.1f; // Примерный интервал обновлений (100 мс)


	public:
		// Get player character ID
		UFUNCTION(BlueprintCallable, Category = "Player")
		int32 GetPlayerCharacterID() const { return playerData.characterData.characterId; }

		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
		TSubclassOf<class UDamageTextWidget> DamageTextWidgetClass;

		public:
			UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
			class UInputAction* PickupAction;


			UFUNCTION(BlueprintCallable, Category = "Player Data")
			FClientDataStruct GetPlayerData() const { return playerData; }

			// Get just the position data from playerData
			UFUNCTION(BlueprintCallable, Category = "Player Data")
			FPositionDataStruct GetPlayerDataPosition() const { return playerData.characterData.characterPosition; }

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
};
