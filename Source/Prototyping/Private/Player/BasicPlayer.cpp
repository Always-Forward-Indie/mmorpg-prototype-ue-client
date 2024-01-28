// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/BasicPlayer.h"
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

    if (MyGameInstance && !PlayerData.bIsOtherClient)
    {
        FVector currentLocation = GetActorLocation();
        float distanceMoved = (currentLocation - FVector(PlayerData.X, PlayerData.Y, PlayerData.Z)).Size();

        // Update the accumulated distance moved based on DeltaTime
        AccumulatedDistance += distanceMoved;

        // Update the time accumulated since the last update was sent
        TimeSinceLastUpdate += DeltaTime;

        // Check if enough time has passed or a significant distance has been covered
        if (
            //TimeSinceLastUpdate >= UpdateInterval && 
            AccumulatedDistance > MovementThreshold
            )
        {
            // Update player coordinates
            PlayerData.X = currentLocation.X;
            PlayerData.Y = currentLocation.Y;
            PlayerData.Z = currentLocation.Z;

            // Send player movement to the game server
            MyGameInstance->SendPlayerMovementToGameServer(PlayerData.ClientSecret, PlayerData.ClientID, PlayerData.CharacterID, PlayerData.X, PlayerData.Y, PlayerData.Z);

            // Reset the accumulated time and distance
            TimeSinceLastUpdate = 0.0f;
            AccumulatedDistance = 0.0f;
        }
    }
}

// Set is other client
void ABasicPlayer::SetIsOtherClient(bool bIsOtherClient)
{
	PlayerData.bIsOtherClient = bIsOtherClient;
}

// Set client ID
void ABasicPlayer::SetClientID(int32 ID)
{
	PlayerData.ClientID = ID;
}

// Set client token
void ABasicPlayer::SetClientSecret(FString Secret)
{
	PlayerData.ClientSecret = Secret;
}

// Set character ID
void ABasicPlayer::SetCharacterID(int32 ID)
{
    PlayerData.CharacterID = ID;
}

// Set client login
void ABasicPlayer::SetClientLogin(FString Login)
{
    PlayerData.ClientLogin = Login;
}

// set player class
void ABasicPlayer::SetPlayerClass(FString Class)
{
    PlayerData.CharacterClassName = Class;
}

// set player race
void ABasicPlayer::SetPlayerRace(FString Race)
{
    PlayerData.CharacterRaceName = Race;
}

// set player name
void ABasicPlayer::SetPlayerName(FString Name)
{
	PlayerData.CharacterName = Name;
}

// set player level
void ABasicPlayer::SetPlayerLevel(int32 Level)
{
	PlayerData.CharacterLevel = Level;
}

// set player experience points
void ABasicPlayer::SetPlayerExpPoints(int32 ExpPoints)
{
	PlayerData.CharacterExpPoints = ExpPoints;
}

// set player current HP points
void ABasicPlayer::SetPlayerCurrentHPPoints(int32 CurrentHPPoints)
{
	PlayerData.CharacterCurrentHPPoints = CurrentHPPoints;
}

// set player current MP points
void ABasicPlayer::SetPlayerCurrentMPPoints(int32 CurrentMPPoints)
{
	PlayerData.CharacterCurrentMPPoints = CurrentMPPoints;
}

// set player coordinates
void ABasicPlayer::SetCoordinates(double x, double y, double z)
{
	PlayerData.X = x;
	PlayerData.Y = y;
	PlayerData.Z = z;
}

void ABasicPlayer::Move(const FInputActionValue& Value)
{
    if (Controller != nullptr)
    {
        const FVector2D MoveValue = Value.Get<FVector2D>();
        const FRotator MovementRotation(0, Controller->GetControlRotation().Yaw, 0);

        // Forward/Backward direction
        if (MoveValue.Y != 0.f)
        {
            // Get forward vector
            const FVector Direction = MovementRotation.RotateVector(FVector::ForwardVector);

            AddMovementInput(Direction, MoveValue.Y);
        }

        // Right/Left direction
        if (MoveValue.X != 0.f)
        {
            // Get right vector
            const FVector Direction = MovementRotation.RotateVector(FVector::RightVector);

            AddMovementInput(Direction, MoveValue.X);
        }
    }
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