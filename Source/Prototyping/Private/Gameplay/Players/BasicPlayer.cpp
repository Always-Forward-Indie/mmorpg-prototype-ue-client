// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "MyGameInstance.h"


void ABasicPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EnhancedInputComponent && InputMappingContext)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnhancedInputComponent init"));
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

        // bind start movement simulation
        EnhancedInputComponent->BindAction(StartMovementSimulationAction, ETriggerEvent::Triggered, this, &ABasicPlayer::StartMovementSimulation);
        // bind stop movement simulation
        EnhancedInputComponent->BindAction(StopMovementSimulationAction, ETriggerEvent::Triggered, this, &ABasicPlayer::StopMovementSimulation);
    }
}

// Sets default values
ABasicPlayer::ABasicPlayer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    //Create audio component
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);

    // Init simulation variables
    SquareCenter = FVector(0.f, 0.f, 90.f); // Assuming Z is up and you want to move around this center
    SideLength = 1000.f; // The length of the side of the square
    TargetPosition = SquareCenter + FVector(SideLength / 2, 0.f, 0.f); // Start with a target position
}

// Called when the game starts or when spawned
void ABasicPlayer::BeginPlay()
{
	Super::BeginPlay();

    MyGameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (MyGameInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("GameInstance found"));
    }
    else
    {
		UE_LOG(LogTemp, Error, TEXT("GameInstance not found"));
	}

	UE_LOG(LogTemp, Warning, TEXT("Player Was Created"));
}

// Called every frame
void ABasicPlayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (MyGameInstance && !playerData.isOtherClient)
    {
        FVector currentLocation = GetActorLocation();
        FRotator currentRotation = GetActorRotation();

        FVector newLocation = FVector(playerData.characterData.characterPosition.positionX,
            			playerData.characterData.characterPosition.positionY,
            			playerData.characterData.characterPosition.positionZ);

        FVector MovementDirection = (currentLocation - newLocation).GetSafeNormal();
        if (!MovementDirection.IsNearlyZero())
        {
            // Calculate the desired rotation based on the movement direction
            FRotator DesiredRotation = MovementDirection.Rotation();
            DesiredRotation.Pitch = 0.0f; // Keep the pitch level, adjust if your game needs vertical aiming
            DesiredRotation.Roll = 0.0f;  // Typically, you don't need to roll the character

            if (!bSimulateMovement) {
                playerData.characterData.characterPosition.rotationZ = DesiredRotation.Yaw;
            }
        }

        // Compare current position and rotation to the last sent values
        bool hasPositionChanged = !currentLocation.Equals(LastSentPosition, PositionThreshold);
        //bool hasRotationChanged = !FMath::IsNearlyEqual(currentRotation.Yaw, LastSentRotation.Yaw, RotationThreshold);

        TimeSinceLastUpdate += DeltaTime;

        if (hasPositionChanged && TimeSinceLastUpdate >= UpdateInterval)
        {
            // Update player data with current state
            playerData.characterData.characterPosition.positionX = currentLocation.X;
            playerData.characterData.characterPosition.positionY = currentLocation.Y;
            playerData.characterData.characterPosition.positionZ = currentLocation.Z;
            //if (!bSimulateMovement) {
                //playerData.characterData.characterPosition.rotationZ = currentRotation.Yaw;
            //}

            // Send player movement to the game server
            MyGameInstance->PlayerManager->SendMovePlayerRequest(playerData);

            // Update the last sent position and rotation
            LastSentPosition = currentLocation;
            LastSentRotation = currentRotation;

            // Reset the timer
            TimeSinceLastUpdate = 0.0f;
        }
    }


    if (bSimulateMovement && !playerData.isOtherClient)
    {
		UpdateMovement(DeltaTime);
	}

    // Move the player if it is an OTHER CLIENT
    if (MyGameInstance && playerData.isOtherClient)
    {
        FVector TargetPointPosition = FVector(playerData.characterData.characterPosition.positionX,
            playerData.characterData.characterPosition.positionY,
            playerData.characterData.characterPosition.positionZ);

        // Target rotation from playerData
        float TargetRotationZ = playerData.characterData.characterPosition.rotationZ;

        FVector CurrentPosition = GetActorLocation();
        FVector Direction = (TargetPointPosition - CurrentPosition).GetSafeNormal();
        float DistanceToTarget = (TargetPointPosition - CurrentPosition).Size();

        if (DistanceToTarget > 0.2f) // Small threshold to prevent jittering
        {
            float MovementStep = MoveSpeed * DeltaTime;
            if (DistanceToTarget < MovementStep) // Close enough to snap to the target
            {
                SetActorLocation(TargetPointPosition);
            }
            else
            {
                FVector NewPosition = CurrentPosition + Direction * MovementStep;
                SetActorLocation(NewPosition);
            }
        }

        // Interpolate rotation
        FRotator CurrentRotation = GetActorRotation();
        FRotator TargetRotation = FRotator(CurrentRotation.Pitch, TargetRotationZ, CurrentRotation.Roll);

        // You can adjust the rotation speed if needed
        float RotationStep = RotationSpeed * DeltaTime; // Define RotationSpeed as needed
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationStep);

        SetActorRotation(NewRotation);
    }
}

// get player data is other client
bool ABasicPlayer::GetIsOtherClient()
{
	return playerData.isOtherClient;
}

// Set message data
void ABasicPlayer::SetMessageData(const FMessageDataStruct NewMessageData)
{
	messageData = NewMessageData;
}
 
// Set is other client
void ABasicPlayer::SetIsOtherClient(bool bIsOtherClient)
{
    playerData.isOtherClient = bIsOtherClient;
}

// Set client ID
void ABasicPlayer::SetClientID(int32 ID)
{
    playerData.clientId = ID;
}

// Set client token
void ABasicPlayer::SetClientSecret(FString Secret)
{
    playerData.hash = Secret;
}

// Set character ID
void ABasicPlayer::SetCharacterID(int32 ID)
{
    playerData.characterData.characterId = ID;
}

// Set client login
void ABasicPlayer::SetClientLogin(FString Login)
{
    playerData.clientLogin = Login;
}

// set player class
void ABasicPlayer::SetPlayerClass(FString Class)
{
    playerData.characterData.characterClass = Class;
}

// set player race
void ABasicPlayer::SetPlayerRace(FString Race)
{
    playerData.characterData.characterRace = Race;
}

// set player name
void ABasicPlayer::SetPlayerName(FString Name)
{
    playerData.characterData.characterName = Name;
}

// set player level
void ABasicPlayer::SetPlayerLevel(int32 Level)
{
    playerData.characterData.characterLevel = Level;
}

// set player experience points
void ABasicPlayer::SetPlayerExpPoints(int32 ExpPoints)
{
    playerData.characterData.characterExperiencePoints = ExpPoints;
}

// set player current HP points
void ABasicPlayer::SetPlayerCurrentHPPoints(int32 CurrentHPPoints)
{
    playerData.characterData.characterCurrentHealth = CurrentHPPoints;
}

// set player current MP points
void ABasicPlayer::SetPlayerCurrentMPPoints(int32 CurrentMPPoints)
{
    playerData.characterData.characterCurrentMana = CurrentMPPoints;
}

// set player coordinates
void ABasicPlayer::SetCoordinates(double x, double y, double z, double rotZ)
{
    playerData.characterData.characterPosition.positionX = x;
    playerData.characterData.characterPosition.positionY = y;
    playerData.characterData.characterPosition.positionZ = z;
    playerData.characterData.characterPosition.rotationZ = rotZ;
}

void ABasicPlayer::Move(const FInputActionValue& Value)
{
    if (Controller != nullptr)
    {
        const FVector2D MoveValue = Value.Get<FVector2D>().GetSafeNormal(); // Normalize input vector
        const FRotator MovementRotation(0, Controller->GetControlRotation().Yaw, 0);

        // Calculate movement direction based on control rotation
        const FVector ForwardDirection = MovementRotation.RotateVector(FVector::ForwardVector);
        const FVector RightDirection = MovementRotation.RotateVector(FVector::RightVector);

        // Calculate movement input based on normalized MoveValue
        const FVector MovementInput = (ForwardDirection * MoveValue.Y + RightDirection * MoveValue.X).GetSafeNormal();

        // Apply movement input with constant speed
        AddMovementInput(MovementInput, MoveSpeed * GetWorld()->GetDeltaSeconds());
    }
}

// Function to convert string to timestamp
FDateTime ABasicPlayer::StringToTimestamp(const FString& DateTimeString) {
    FDateTime DateTime;
    FDateTime::Parse(DateTimeString, DateTime);
    return DateTime;
}

// Function to interpolate player position
FVector ABasicPlayer::InterpolatePlayerPosition(const FClientDataStruct& ClientData, const FMessageDataStruct& MessageData)
{
    // convert message data timestamp from string to FDateTime
    FDateTime PacketDateTime = StringToTimestamp(MessageData.timestamp);
    // get seconds from the timestamp
    float ServerTimestamp = PacketDateTime.GetSecond();

    FVector LastKnownPosition = GetActorLocation();
    FVector NewPosition = FVector(ClientData.characterData.characterPosition.positionX, 
        ClientData.characterData.characterPosition.positionY, 
        ClientData.characterData.characterPosition.positionZ);
    // Implement interpolation logic here (e.g., linear interpolation)
    // For simplicity, linear interpolation is used in this example
    float Alpha = CalculateInterpolationAlpha(ServerTimestamp);
    return FMath::Lerp(LastKnownPosition, NewPosition, Alpha);
}

// Function to calculate interpolation alpha based on server timestamp
float ABasicPlayer::CalculateInterpolationAlpha(float ServerTimestamp)
{
    float InterpolationDuration = 5.1f; // Adjust as needed
    //float CurrentTime = GetWorld()->GetTimeSeconds();
    float CurrentTime = FDateTime::UtcNow().GetSecond();
    float Alpha = FMath::Clamp((CurrentTime - ServerTimestamp) / InterpolationDuration, 0.0f, 1.0f);

    //log server timestamp
    UE_LOG(LogTemp, Warning, TEXT("ServerTimestamp: %f"), ServerTimestamp);
    //log current time
    UE_LOG(LogTemp, Warning, TEXT("CurrentTimestamp: %f"), CurrentTime);
    // log alpha value
    UE_LOG(LogTemp, Warning, TEXT("Alpha: %f"), Alpha);

    return Alpha;
}

// Function to predict player position
FVector ABasicPlayer::PredictPlayerPosition(const FClientDataStruct& ClientData, float DeltaTime)
{
    FVector LastKnownPosition = GetActorLocation();
    FVector NewPosition = FVector(ClientData.characterData.characterPosition.positionX,
        ClientData.characterData.characterPosition.positionY,
        ClientData.characterData.characterPosition.positionZ);
    // Implement prediction logic here (e.g., dead reckoning)
    // For simplicity, linear prediction is used in this example
    FVector PredictedPosition = NewPosition + (NewPosition - LastKnownPosition) / DeltaTime;
    return PredictedPosition;
}

void ABasicPlayer::Look(const FInputActionValue& Value)
{
    if (Controller != nullptr)
    {
        const FVector2D LookValue = Value.Get<FVector2D>();

        if (LookValue.X != 0.f)
        {
            AddControllerYawInput(LookValue.X);
        }

        if (LookValue.Y != 0.f)
        {
            AddControllerPitchInput(LookValue.Y*-1);
        }
    }
}

// Play sound
void ABasicPlayer::PlaySound(USoundBase* Sound)
{
    AudioComponent->SetSound(Sound);
    AudioComponent->Play();
}

// Stop sound
void ABasicPlayer::StopSound()
{
    AudioComponent->Stop();
}

void ABasicPlayer::StartMovementSimulation()
{
    bSimulateMovement = true;
}

void ABasicPlayer::StopMovementSimulation()
{
    bSimulateMovement = false;
}

void ABasicPlayer::UpdateMovement(float DeltaTime)
{
    FVector CurrentLocation = GetActorLocation();
    float DistanceToTarget = FVector::Dist(CurrentLocation, TargetPosition);

    if (DistanceToTarget < 50.f) // Threshold to decide when to pick a new target
    {
        float NewX = FMath::FRandRange(SquareCenter.X - SideLength / 2, SquareCenter.X + SideLength / 2);
        float NewY = FMath::FRandRange(SquareCenter.Y - SideLength / 2, SquareCenter.Y + SideLength / 2);
        TargetPosition = FVector(NewX, NewY, 90.0f); // Assuming Z is constant for this example

        //float RandomYaw = FMath::FRandRange(0.0f, 360.0f);

        FVector MovementDirection = (TargetPosition - CurrentLocation).GetSafeNormal();
        if (!MovementDirection.IsNearlyZero())
        {
            // Calculate the desired rotation based on the movement direction
            FRotator DesiredRotation = MovementDirection.Rotation();
            DesiredRotation.Pitch = 0.0f; // Keep the pitch level, adjust if your game needs vertical aiming
            DesiredRotation.Roll = 0.0f;  // Typically, you don't need to roll the character

            playerData.characterData.characterPosition.rotationZ = DesiredRotation.Yaw;
        }


        // Logging for debugging
        UE_LOG(LogTemp, Warning, TEXT("New target position: %s"), *TargetPosition.ToString());
    }

    // Move towards TargetPosition if not already close
    if (DistanceToTarget > 1.0f) // Use a small threshold to avoid jittering at the target location
    {
        FVector Direction = (TargetPosition - CurrentLocation).GetSafeNormal();
        float MovementStep = MoveSpeed * DeltaTime;
        FVector NewPosition = CurrentLocation + Direction * MovementStep;
        SetActorLocation(NewPosition);
    }
}