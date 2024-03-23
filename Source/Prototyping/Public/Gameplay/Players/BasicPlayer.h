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
#include "BasicPlayer.generated.h"


class UMyGameInstance;

UCLASS()
class PROTOTYPING_API ABasicPlayer : public ACharacter
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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Sets default values for this character's properties
	ABasicPlayer();

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
	void UpdateRemotePlayerMovementOld(float DeltaTime);
	// Update REMOTE player movement
	void UpdateRemotePlayerMovement();
	// Interpolate movement for REMOTE player
	float CalculateInterpolationSpeed(float MovementSpeed);
	void InterpolateMovement(float DeltaTime, float InterpolationSpeed);

	// Set Is Other Client
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetIsOtherClient(bool bIsOtherClient);

	// Set client ID
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetClientID(int32 ID);

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

	// set player current HP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerCurrentHPPoints(int32 CurrentHPPoints);

	// set player current MP points
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetPlayerCurrentMPPoints(int32 CurrentMPPoints);

	// Set message data
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	void SetMessageData(const FMessageDataStruct NewMessageData);

	// Get is other client
	UFUNCTION(BlueprintCallable, Category = "Player Data")
	bool GetIsOtherClient();


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
};
