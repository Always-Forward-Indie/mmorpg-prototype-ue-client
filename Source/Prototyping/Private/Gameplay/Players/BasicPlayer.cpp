// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "EngineUtils.h"
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
    
    
        // Bind attack action (you'll need to add AttackAction to your header file)
        EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnAttackInput);
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

//Create HUD
void ABasicPlayer::CreateHUD()
{
	if (HUDWidgetClass)
	{
		if (!GetIsOtherClient()) {
			//debug 
			UE_LOG(LogTemp, Warning, TEXT("Creating Player HUD Widget"));
			this->PlayerHUD = CreateWidget<UPlayerHUD>(GetWorld(), HUDWidgetClass);
			if (PlayerHUD)
			{
				PlayerHUD->AddToViewport();

                APlayerController* PC = Cast<APlayerController>(GetController());

                if (PlayerHUD->GetDamageCanvas() && PC) {
                    MyGameInstance->GetUIManager()->Init(PC, PlayerHUD->GetDamageCanvas(), DamageTextWidgetClass);
                }
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HUDWidgetClass is not set in ABasicPlayer!"));
	}
}


// update HUD

void ABasicPlayer::UpdateHUD()
{
    if (PlayerHUD)
    {
        if (!GetIsOtherClient())
        {
            //debug 
			//UE_LOG(LogTemp, Warning, TEXT("Updating Player HUD for player with id"));

            //get player max HP, Mana and XP from attributes
            float MaxHealth = 1.0f;
            float MaxMana = 1.0f;

            // Проверяем, есть ли в attributesData нужные ключи
            if (const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health")))
            {
                MaxHealth = HealthAttr->attributeValue;

                //UE_LOG(LogTemp, Warning, TEXT("Max Player Health: %f"), MaxHealth);
                //UE_LOG(LogTemp, Warning, TEXT("Current Player Health: %d"), playerData.characterData.characterCurrentHealth);
            }

            if (const FAttributeDataStruct* ManaAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana")))
            {
                MaxMana = ManaAttr->attributeValue;

                //UE_LOG(LogTemp, Warning, TEXT("Max Player Mana: %f"), MaxMana);
                //UE_LOG(LogTemp, Warning, TEXT("Current Player Health: %d"), playerData.characterData.characterCurrentMana);
            }

            // Update HUD
            PlayerHUD->SetHP(playerData.characterData.characterCurrentHealth / MaxHealth);
            PlayerHUD->SetMana(playerData.characterData.characterCurrentMana / MaxMana);
            PlayerHUD->SetXP(playerData.characterData.characterExperiencePoints / playerData.characterData.characterExpForNextLevel);
            PlayerHUD->SetLevel(playerData.characterData.characterLevel);
        }
    }
}

// Called every frame
void ABasicPlayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    FVector CurrentLocation = GetActorLocation();
    playerData.characterData.bIsMoving = !CurrentLocation.Equals(LastFrameLocation, 1.0f);
    LastFrameLocation = CurrentLocation;

    if (MyGameInstance && !playerData.isOtherClient)
    {
        // Update player movement for local player
		UpdateCurrentPlayerMovement(DeltaTime);
    }

    // Update player movement for remote player
    if (playerData.isOtherClient)
    {
        // UpdateRemotePlayerMovementOld(DeltaTime);
        UpdateRemotePlayerMovement();
    }

    // Simulate movement for local player
    if (bSimulateMovement && !playerData.isOtherClient)
    {
        UpdateMovementSimulation(DeltaTime);
	}

	//if character data is not empty
	if (playerData.characterData.characterId != 0) {
		// Update the HUD
		UpdateHUD();
	}

    CheckForMOB();
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
            AddControllerPitchInput(LookValue.Y * -1);
        }
    }
}

void ABasicPlayer::UpdateCurrentPlayerMovement(float DeltaTime)
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

        // Send player movement to the game server
        MyGameInstance->PlayerManager->SendMovePlayerRequest(playerData);

        // Update the last sent position and rotation
        LastSentPosition = currentLocation;
        LastSentRotation = currentRotation;

        // Reset the timer
        TimeSinceLastUpdate = 0.0f;
    }
}


void ABasicPlayer::UpdateRemotePlayerMovement()
{
    TimeSinceLastPositionUpdate += GetWorld()->GetDeltaSeconds();

    float LerpFactor = FMath::Clamp(TimeSinceLastPositionUpdate / ServerPositionUpdateInterval, 0.f, 1.f);

    FVector NewPosition = FMath::Lerp(LastReceivedPosition, TargetReceivedPosition, LerpFactor);
    // Плавно интерполируем вращение независимо от позиции, с умеренной скоростью
    float RotationInterpSpeed = 15.0f; // Персонаж повернется на 15 градусов за секунду

    FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetReceivedRotation, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed);



    float distanceToTarget = FVector::Dist(GetActorLocation(), TargetReceivedPosition);

    if (distanceToTarget > 300.f) {
        SetActorLocation(TargetReceivedPosition);
    }
    else {
        SetActorLocation(NewPosition);
    }

    SetActorRotation(NewRotation);
}

float ABasicPlayer::CalculateRotationInterpSpeed()
{
    float AngleDifference = FMath::Abs(LastReceivedRotation.Yaw - TargetReceivedRotation.Yaw);
    AngleDifference = FMath::Min(AngleDifference, 360.f - AngleDifference); // Учитываем переходы через 360°
    return AngleDifference / ServerPositionUpdateInterval;
}


float ABasicPlayer::CalculateInterpolationSpeed(float MovementSpeed)
{
    float InterpolationSpeed = MovementSpeed * interpolationSpeedFactor;
    return FMath::Clamp(InterpolationSpeed, 1.0f, maxInterpolationSpeed);
}

FDateTime ABasicPlayer::StringToTimestamp(const FString& DateTimeString) {
    FDateTime DateTime;
    FDateTime::Parse(DateTimeString, DateTime);
    return DateTime;
}

// get player data is other client
bool ABasicPlayer::GetIsOtherClient()
{
	return playerData.isOtherClient;
}

//get is dead state
bool ABasicPlayer::GetIsDead() const
{
	return playerData.characterData.bIsDead;
}

// get is moving state
bool ABasicPlayer::GetIsMoving() const
{
    return playerData.characterData.bIsMoving;
}

// get current zone name
FString ABasicPlayer::GetCurrentZoneName()
{
	return CurrentZoneName;
}

// get player current HP points
int32 ABasicPlayer::GetPlayerCurrentHPPoints() const
{
    return playerData.characterData.characterCurrentHealth;
}

// get player current MP points
int32 ABasicPlayer::GetPlayerCurrentMPPoints() const
{
    return playerData.characterData.characterCurrentMana;
}

// Set message data
void ABasicPlayer::SetMessageData(const FMessageDataStruct NewMessageData)
{
	messageData = NewMessageData;
}

void ABasicPlayer::SetCurrentZoneName(const FString& NewZoneName)
{
    CurrentZoneName = NewZoneName;
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

// set player next level exp
void ABasicPlayer::SetPlayerNextLevelExp(int32 NextLevelExp)
{
	playerData.characterData.characterExpForNextLevel = NextLevelExp;
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

// set player attributes
void ABasicPlayer::SetPlayerAttributes(TMap<FString, FAttributeDataStruct> Attributes)
{
	playerData.characterData.characterAttributes.attributesData = Attributes;

	//debug player attributes
	for (auto& Elem : Attributes)
	{
		FString Key = Elem.Key;
		FAttributeDataStruct Value = Elem.Value;
		UE_LOG(LogTemp, Warning, TEXT("Player Attribute Key: %s, Value: %d"), *Key, Value.attributeValue);
	}

}

// set player coordinates
void ABasicPlayer::SetCoordinates(double x, double y, double z, double rotZ)
{
    playerData.characterData.characterPosition.positionX = x;
    playerData.characterData.characterPosition.positionY = y;
    playerData.characterData.characterPosition.positionZ = z;
    playerData.characterData.characterPosition.rotationZ = rotZ;


    LastReceivedPosition = GetActorLocation();
    TargetReceivedPosition = FVector(x, y, z);

    LastReceivedRotation = GetActorRotation();
    TargetReceivedRotation = FRotator(0, rotZ, 0);

    TimeSinceLastPositionUpdate = 0.0f;
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

void ABasicPlayer::UpdateMovementSimulation(float DeltaTime)
{
    FVector CurrentLocation = GetActorLocation();
    float DistanceToTarget = FVector::Dist(CurrentLocation, TargetPosition);

    if (DistanceToTarget < 50.f) // Threshold to decide when to pick a new target
    {
        float NewX = FMath::FRandRange(SquareCenter.X - SideLength / 2, SquareCenter.X + SideLength / 2);
        float NewY = FMath::FRandRange(SquareCenter.Y - SideLength / 2, SquareCenter.Y + SideLength / 2);
        TargetPosition = FVector(NewX, NewY, 90.0f); // Assuming Z is constant for this example

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
        float MovementStep = MoveSpeed * 2 * DeltaTime; // Adjust as needed
        //float MovementStep = MoveSpeed* GetWorld()->GetDeltaSeconds();
        FVector NewPosition = CurrentLocation + Direction * MovementStep;
        SetActorLocation(NewPosition);
    }
}

void ABasicPlayer::CheckForMOB()
{
    // Use the player's forward direction instead of the camera
    FVector Start = GetActorLocation();
    FVector ForwardVector = GetActorForwardVector();
    FVector End = Start + ForwardVector * 1000.0f;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    float CapsuleRadius = 50.0f;
    float CapsuleHalfHeight = 100.0f;

    TArray<FHitResult> HitResults;

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, Start, End, FQuat::Identity,
        ECC_Visibility, FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
        Params
    );

    //DrawDebugLine(
    //    GetWorld(),
    //    Start,
    //    End,
    //    FColor::Green,
    //    false,
    //    2.0f,
    //    0,
    //    1.0f
    //);

    ABasicMOB* ClosestMob = nullptr;
    float ClosestDistance = FLT_MAX;

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            ABasicMOB* Mob = Cast<ABasicMOB>(Hit.GetActor());
            if (Mob)
            {
                float Distance = (Hit.ImpactPoint - Start).Size();
                if (Distance < ClosestDistance)
                {
                    ClosestDistance = Distance;
                    ClosestMob = Mob;
                }
            }
        }
    }

    for (TActorIterator<ABasicMOB> It(GetWorld()); It; ++It)
    {
        if (*It == ClosestMob)
        {
            It->MobHeadInfo->ShowWidget(true);
        }
        else
        {
            It->MobHeadInfo->ShowWidget(false);
        }
    }
}


void ABasicPlayer::AttackTarget(int32 TargetID, int32 ActionID, bool bUseAI, const FString& TargetType)
{
    if (!MyGameInstance || !MyGameInstance->GetPlayerManager())
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: MyGameInstance or PlayerManager not found"));
        return;
    }

    if (TargetID <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot attack: Invalid target ID"));
        return;
    }

    MyGameInstance->GetPlayerManager()->SendPlayerAttackRequest(playerData, TargetID, ActionID, bUseAI, TargetType);
    UE_LOG(LogTemp, Warning, TEXT("Player attacking target ID: %d with action ID: %d, target type: %s"), TargetID, ActionID, *TargetType);
}


void ABasicPlayer::AttackActor(AActor* TargetActor, int32 ActionID, bool bUseAI)
{
    if (!TargetActor || TargetActor == this)
        return;

    if (ABasicMOB* Mob = Cast<ABasicMOB>(TargetActor))
    {
        int32 MobID = FCString::Atoi(*Mob->GetMOBUId());
        AttackTarget(MobID, ActionID, bUseAI, TEXT("MOB"));
    }
    else if (ABasicPlayer* TargetPlayer = Cast<ABasicPlayer>(TargetActor))
    {
        if (!TargetPlayer->GetIsOtherClient())
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot attack local player or self."));
            return;
        }

        int32 TargetPlayerID = TargetPlayer->GetPlayerCharacterID();
        AttackTarget(TargetPlayerID, ActionID, bUseAI, TEXT("Player"));
    }
}

void ABasicPlayer::OnAttackInput()
{
    FVector Start = GetActorLocation();
    FVector ForwardVector = GetActorForwardVector();
    FVector End = Start + ForwardVector * 1000.0f;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    float CapsuleRadius = 50.0f;
    float CapsuleHalfHeight = 100.0f;

    TArray<FHitResult> HitResults;

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, Start, End, FQuat::Identity,
        ECC_Visibility, FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
        Params
    );

    DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 1.0f);

    if (!bHit || HitResults.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid target found for attack"));
        return;
    }

    FHitResult* ClosestHit = nullptr;
    float MinDistSq = FLT_MAX;

    for (FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || HitActor == this)
            continue;

        bool bValid =
            Cast<ABasicMOB>(HitActor) ||
            (Cast<ABasicPlayer>(HitActor) && Cast<ABasicPlayer>(HitActor)->GetIsOtherClient());

        if (!bValid)
            continue;

        float DistSq = FVector::DistSquared(GetActorLocation(), Hit.ImpactPoint);
        if (DistSq < MinDistSq)
        {
            MinDistSq = DistSq;
            ClosestHit = &Hit;
        }
    }

    if (ClosestHit)
    {
        DrawDebugSphere(GetWorld(), ClosestHit->ImpactPoint, 25, 12, FColor::Red, false, 1.0f);
        AttackActor(ClosestHit->GetActor(), 1, false);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid MOB or player to attack"));
    }
}
