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

	float TimeSinceLastUpdate;

	FVector LastSentPosition;
	FRotator LastSentRotation;

	// rotation threshold
	float RotationThreshold = 5.0f;
	// distance threshold
	float PositionThreshold = 5.0f;


	float AccumulatedDistance;


	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	float MovementThreshold = 100.0f;
	const float UpdateInterval = 0.5f;

	// Constant movement speed
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	float MoveSpeed = 200.0f; // Adjust as needed

	// Constant rotation speed
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	float RotationSpeed = 250.0f; // Adjust as needed


	//SIMULATION OF MOVEMENT VARIABLES
	// Variables for circular movement
	FVector CenterPosition;
	float Radius;
	float CurrentAngle;

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

	//SIMULATION OF MOVEMENT FUNCTIONS
	void StartMovementSimulation();
	void StopMovementSimulation();
	void UpdateMovement(float DeltaTime);



	// Sets default values for this character's properties
	ABasicPlayer();

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

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

	// Enhanced Input movement functions
	void Move(const FInputActionValue& Value);
	FDateTime StringToTimestamp(const FString& DateTimeString);
	FVector InterpolatePlayerPosition(const FClientDataStruct& ClientData, const FMessageDataStruct& MessageData);
	float CalculateInterpolationAlpha(float ServerTimestamp);
	FVector PredictPlayerPosition(const FClientDataStruct& MovementData, float DeltaTime);
	void Look(const FInputActionValue& Value);

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
