// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/BasicMOB.h"

// Sets default values
ABasicMOB::ABasicMOB()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ABasicMOB::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABasicMOB::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MOBData.mobID != 0)
	{
		UpdatePosition();
	}

}

// Called to bind functionality to input
void ABasicMOB::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


// set mob name text
void ABasicMOB::SetMobNameText(UTextRenderComponent* MobNameTextComponent, const FString& Name)
{
	//get text render component and set text
	//UTextRenderComponent* MobNameText = FindComponentByClass<UTextRenderComponent>();

	if (MobNameTextComponent)
	{
		MobNameTextComponent->SetText(FText::FromString(Name));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MobNameText not found"));
	}
}

// set mob level text
void ABasicMOB::SetMobLevelText(UTextRenderComponent* MobLevelTextComponent, const FString& Level)
{
	//get text render component and set text
	//UTextRenderComponent* MobLevelText = FindComponentByClass<UTextRenderComponent>();
	FText LevelText = FText::FromString("LVL: " + Level);

	if (MobLevelTextComponent)
	{
		MobLevelTextComponent->SetText(LevelText);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MobLevelText not found"));
	}
}

void ABasicMOB::SetMOBTag(const FString& Tag)
{
	Tags.Add(FName(*Tag));
}

FMOBStruct ABasicMOB::GetMOBData() const
{
	return MOBData;
}

void ABasicMOB::SetMOBData(const FMOBStruct& Data)
{
	if (MOBData.mobID == 0)
	{
		MOBData = Data;
		MOBDataUpdated.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MOB Data already set"));
	}
}

void ABasicMOB::SetMOBId(const int& MOBId)
{
	MOBData.mobID = MOBId;
}

void ABasicMOB::SetMOBUId(const FString& MOBUId)
{
	MOBData.mobUniqueID = MOBUId;
}

void ABasicMOB::SetMOBZoneId(const int& MOBZoneId)
{
	MOBData.mobZoneID = MOBZoneId;
}

void ABasicMOB::SetMOBName(const FString& MOBName)
{
	MOBData.mobName = MOBName;
}

void ABasicMOB::SetMOBRace(const FString& MOBRace)
{
	MOBData.mobRace = MOBRace;
}

void ABasicMOB::SetMOBLevel(const int& MOBLevel)
{
	MOBData.mobLevel = MOBLevel;
}

void ABasicMOB::SetMOBCurrentHealth(const int& MOBCurrentHealth)
{
	MOBData.mobCurrentHealth = MOBCurrentHealth;
}

void ABasicMOB::SetMOBCurrentMana(const int& MOBCurrentMana)
{
	MOBData.mobCurrentMana = MOBCurrentMana;
}

void ABasicMOB::SetMOBPosition(const FPositionDataStruct& MOBPosition)
{
	MOBData.mobPosition = MOBPosition;
}

void ABasicMOB::SetMOBAttributes(const FAttributesDataStruct& MOBAttributes)
{
	MOBData.mobAttributes = MOBAttributes;
}

void ABasicMOB::SetMOBAttribute(const FString& AttributeSlug, const FAttributeDataStruct& MOBAttribute)
{
	MOBData.mobAttributes.attributesData.Add(AttributeSlug, MOBAttribute);
}

void ABasicMOB::SetMOBIsDead(const bool& bIsDead)
{
	MOBData.bIsDead = bIsDead;
}

void ABasicMOB::SetMOBIsAggressive(const bool& bIsAggressive)
{
	MOBData.bIsAggressive = bIsAggressive;
}

void ABasicMOB::SetLastTimestamp(const FString& Timestamp)
{
	lastTimestamp = Timestamp;
}

FString ABasicMOB::GetMobName() const
{
	return MOBData.mobName;
}

FString ABasicMOB::GetMOBUId() const
{
	return MOBData.mobUniqueID;
}

int ABasicMOB::GetMOBId() const
{
	return MOBData.mobID;
}

FString ABasicMOB::GetMOBRace() const
{
	return MOBData.mobRace;
}

int ABasicMOB::GetMOBLevel() const
{
	return MOBData.mobLevel;
}

int ABasicMOB::GetMOBCurrentHealth() const
{
	return MOBData.mobCurrentHealth;
}

int ABasicMOB::GetMOBCurrentMana() const
{
	return MOBData.mobCurrentMana;
}

FVector ABasicMOB::GetMOBPosition() const
{
	return FVector(MOBData.mobPosition.positionX, MOBData.mobPosition.positionY, MOBData.mobPosition.positionZ);
}

FAttributesDataStruct ABasicMOB::GetMOBAttributes() const
{
	return MOBData.mobAttributes;
}

bool ABasicMOB::GetMOBIsDead() const
{
	return MOBData.bIsDead;
}

bool ABasicMOB::GetMOBIsAggressive() const
{
	return MOBData.bIsAggressive;
}

void ABasicMOB::MOBTextFaceToCamera(UTextRenderComponent* MobTextComponent)
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController)
	{
		APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		FVector CameraLocation = CameraManager->GetCameraLocation();

		// Assuming MyTextRenderer is your TextRenderComponent
		FVector TextLocation = MobTextComponent->GetComponentLocation();

		FRotator LookAtRotation = (CameraLocation - TextLocation).Rotation();
		MobTextComponent->SetWorldRotation(LookAtRotation);
	}
}


void ABasicMOB::UpdatePosition()
{
	// Convert server timestamp to Unix timestamp
	float ServerUnixTimestamp = StringToTimestamp(lastTimestamp).ToUnixTimestamp();

	// Get current world time (Unix timestamp)
	float CurrentWorldUnixTimestamp = GetWorld()->GetTimeSeconds();

	// Calculate elapsed time since last update
	float DeltaTime = CurrentWorldUnixTimestamp - LastUpdateTime;
	LastUpdateTime = CurrentWorldUnixTimestamp;

	// Calculate interpolation speed based on movement speed
	float InterpolationSpeed = CalculateInterpolationSpeed(MoveSpeed);

	// Interpolate movement based on timestamps and elapsed time
	InterpolateMovement(DeltaTime, InterpolationSpeed);
}

// Define a function to calculate interpolation speed based on movement speed
float ABasicMOB::CalculateInterpolationSpeed(float MovementSpeed)
{
	// Calculate interpolation speed based on movement speed
	float InterpolationSpeed = MovementSpeed * interpolationSpeedFactor;

	// Ensure interpolation speed is within reasonable bounds
	InterpolationSpeed = FMath::Clamp(InterpolationSpeed, 1.0f, maxInterpolationSpeed); // MAX_INTERPOLATION_SPEED is a constant defining the maximum interpolation speed

	return InterpolationSpeed;
}

// Function to interpolate movement based on timestamps and elapsed time
void ABasicMOB::InterpolateMovement(float DeltaTime, float InterpolationSpeed)
{
	// Get current position
	FVector CurrentLocation = GetActorLocation();

	// Get target position from playerData
	FVector targetPosition = FVector(MOBData.mobPosition.positionX,
		MOBData.mobPosition.positionY,
		MOBData.mobPosition.positionZ);

	// Interpolate position
	FVector CurrentPosition = GetActorLocation();
	FVector InterpolatedPosition = FMath::VInterpConstantTo(CurrentPosition, targetPosition, DeltaTime, InterpolationSpeed);
	SetActorLocation(InterpolatedPosition);


	// Calculate the movement direction
	FVector MovementDirection = (targetPosition - CurrentLocation).GetSafeNormal();
	if (!MovementDirection.IsNearlyZero())
	{
		// Calculate the desired rotation based on the movement direction
		FRotator DesiredRotation = MovementDirection.Rotation();
		DesiredRotation.Pitch = 0.0f; // Keep the pitch level, adjust if your game needs vertical aiming
		DesiredRotation.Roll = 0.0f;  // Typically, you don't need to roll the character

		// Set the rotation value according direction
		MOBData.mobPosition.rotationZ = DesiredRotation.Yaw;
	}

	// Interpolate rotation
	FRotator CurrentRotation = GetActorRotation();
	FRotator TargetRotation(0, MOBData.mobPosition.rotationZ, 0);

	// Interpolate rotation
	float RotationStep = RotationSpeed * DeltaTime; // Define RotationSpeed as needed
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationStep);
	SetActorRotation(NewRotation);
}

// Function to convert string to timestamp
FDateTime ABasicMOB::StringToTimestamp(const FString& DateTimeString) {
	FDateTime DateTime;
	FDateTime::Parse(DateTimeString, DateTime);
	return DateTime;
}
