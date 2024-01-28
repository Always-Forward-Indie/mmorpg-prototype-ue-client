// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/AudioComponent.h"
//#include "MyGameInstance.h"
#include <Kismet/GameplayStatics.h>
#include "BasicPlayer.generated.h"


class UMyGameInstance;

USTRUCT(BlueprintType)
struct FPlayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	bool bIsOtherClient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 ClientID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	FString ClientSecret;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	FString ClientLogin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 CharacterID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	FString CharacterRaceName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	FString CharacterClassName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 CharacterLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 CharacterExpPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 CharacterCurrentHPPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 CharacterCurrentMPPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	double X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	double Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	double Z;

	// Add other player-related fields here
};

USTRUCT(BlueprintType)
struct FCharacterItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 ClientID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	int32 CharacterID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Data")
	FString CharacterName;
};

UCLASS()
class PROTOTYPING_API ABasicPlayer : public ACharacter
{
	GENERATED_BODY()

private:
	// player data
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Data", meta = (AllowPrivateAccess = "true"))
	FPlayerData PlayerData;

	UMyGameInstance* MyGameInstance;

	float TimeSinceLastUpdate;
	float AccumulatedDistance;
	const float MovementThreshold = 5.0f;
	const float UpdateInterval = 0.5f;

public:
	// Sets default values for this character's properties
	ABasicPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Set Is Other Client
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetIsOtherClient(bool bIsOtherClient);

	// Set client ID
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetClientID(int32 ID);

	// Set client token
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetClientSecret(FString Secret);

	// Set character ID
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetCharacterID(int32 ID);

	// Set client login
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetClientLogin(FString Login);

	// set player coordinates
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetCoordinates(double x, double y, double z);

	// set player class
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerClass(FString Class);

	// set player race
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerRace(FString Race);

	// set player name
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerName(FString Name);

	// set player level
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerLevel(int32 Level);

	// set player experience points
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerExpPoints(int32 ExpPoints);

	// set player current HP points
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerCurrentHPPoints(int32 CurrentHPPoints);

	// set player current MP points
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void SetPlayerCurrentMPPoints(int32 CurrentMPPoints);

	// Enhanced Input movement functions
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* InputMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
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
