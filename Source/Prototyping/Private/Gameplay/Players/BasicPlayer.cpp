// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "UI/UIManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Utils/PlayerAttributeParser.h"

// Implementation of missing input methods
void ABasicPlayer::OnAttackInput()
{
    UE_LOG(LogTemp, Warning, TEXT("Attack input pressed"));
    
    // Look for nearby MOBs to attack
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABasicMOB::StaticClass(), FoundActors);
    
    ABasicMOB* ClosestMob = nullptr;
    float ClosestDistance = 500.0f; // Attack range
    
    for (AActor* Actor : FoundActors)
    {
        ABasicMOB* Mob = Cast<ABasicMOB>(Actor);
        if (Mob && !Mob->GetMOBIsDead())
        {
            float Distance = FVector::Dist(GetActorLocation(), Mob->GetActorLocation());
            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestMob = Mob;
            }
        }
    }
    
    if (ClosestMob)
    {
        //convert mob uid to int
		int32 MobId = FCString::Atoi(*ClosestMob->GetMOBUId());


        // Attack the closest mob with a basic attack skill
        AttackTarget(MobId, CurrentSkillName, 3);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No MOBs in range to attack"));
    }
}

void ABasicPlayer::OnPickupInput()
{
    UE_LOG(LogTemp, Warning, TEXT("Pickup input pressed"));
    
    if (InventoryManager)
    {
        InventoryManager->PickupNearbyItem();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Inventory manager not found"));
    }
}

void ABasicPlayer::OnInventoryToggle()
{
    UE_LOG(LogTemp, Warning, TEXT("Inventory toggle pressed"));
    
    if (InventoryManager)
    {
        InventoryManager->ToggleInventoryUI();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Inventory manager not found"));
    }
}

void ABasicPlayer::OnHarvestInput()
{
    UE_LOG(LogTemp, Warning, TEXT("Harvest input pressed"));
    
    if (!MyGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("MyGameInstance not found"));
        return;
    }
    
    UHarvestManager* HarvestManager = MyGameInstance->GetHarvestManager();
    if (HarvestManager)
    {
        HarvestManager->TryHarvestNearbyCorpse();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Harvest manager not found"));
    }
}

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
    
    
        if (AttackAction)
        {
            // Bind attack action (you'll need to add AttackAction to your header file)
            EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnAttackInput);
        }

        // Pickup item action
        if (PickupAction)
        {
            EnhancedInputComponent->BindAction(PickupAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnPickupInput);
        }

        // Inventory action
        if (InventoryAction)
        {
            EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnInventoryToggle);
        }

        // Harvest action
        if (HarvestAction)
        {
            EnhancedInputComponent->BindAction(HarvestAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnHarvestInput);
        }
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

    // Create UI Manager component
	UIManager = CreateDefaultSubobject<UUIManager>(TEXT("UIManager"));

	// Create Inventory Manager component  
	InventoryManager = CreateDefaultSubobject<UInventoryManager>(TEXT("InventoryManager"));

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

        // Register with combat system if this is the local player
        if (!playerData.isOtherClient)
        {
            UCombatSystemManager* CombatManager = MyGameInstance->GetCombatSystemManager();
            if (CombatManager && GetActorId_Implementation() > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->RegisterCombatable(CombatableInterface);
                UE_LOG(LogTemp, Warning, TEXT("Player %d registered with combat system"), GetActorId_Implementation());
            }
        }
    }
    else
    {
		UE_LOG(LogTemp, Error, TEXT("GameInstance not found"));
	}

	UE_LOG(LogTemp, Warning, TEXT("Player Was Created"));

	// Initialize inventory manager for local player only
	if (!playerData.isOtherClient && MyGameInstance)
	{
		// Set up inventory manager
		if (InventoryManager)
		{
			InventoryManager->SetWorldContext(GetWorld());
			InventoryManager->SetGameInstance(MyGameInstance);
			
			// Get network manager from game instance and initialize inventory
			if (UNetworkManager* NetworkManager = MyGameInstance->GetNetworkManager())
			{
				InventoryManager->Initialize(NetworkManager);
				InventoryManager->SubscribeToNetworkManager();
			}
			
			// Set reference in game instance for easy access
			MyGameInstance->SetInventoryManager(InventoryManager);
		}

		// Initialize harvest manager
		if (UHarvestManager* HarvestManager = MyGameInstance->GetHarvestManager())
		{
			HarvestManager->SetWorldContext(GetWorld());
			HarvestManager->SetGameInstance(MyGameInstance);

			// Get network manager from game instance and initialize harvest
			if (UNetworkManager* NetworkManager = MyGameInstance->GetNetworkManager())
			{
				HarvestManager->Initialize(NetworkManager);
				HarvestManager->SubscribeToNetworkManager();
			}

			UE_LOG(LogTemp, Warning, TEXT("HarvestManager initialized for local player"));
		}

		// Initialize UI manager with slight delay to ensure everything is ready
		if (UIManager)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				if (InventoryManager)
				{
					// Get HarvestManager from GameInstance
					UHarvestManager* HarvestManager = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;

					//get ExperienceManager from GameInstance
					UExperienceManager* ExperienceManager = MyGameInstance ? MyGameInstance->GetExperienceManager() : nullptr;
					
					// Initialize UIManager with both managers
					UIManager->Initialize(InventoryManager, HarvestManager, ExperienceManager);
					
					UE_LOG(LogTemp, Warning, TEXT("UIManager initialized with InventoryManager and HarvestManager"));

					// Initialize experience widget for this character and set initial progression data
					if (ExperienceManager && playerData.characterData.characterId > 0)
					{
						// Create initial progression data from current player data
						FPlayerProgressionStruct InitialProgression;
						InitialProgression.characterId = playerData.characterData.characterId;
						InitialProgression.currentLevel = playerData.characterData.characterLevel;
						InitialProgression.currentExperience = playerData.characterData.characterExperiencePoints;
						InitialProgression.totalExperience = playerData.characterData.characterExperiencePoints;
						InitialProgression.expForNextLevel = playerData.characterData.characterExpForNextLevel;
						InitialProgression.expForCurrentLevel = 0; // Will be calculated by server
						InitialProgression.bHasPendingLevelUp = false;
						InitialProgression.pendingLevelGained = 0;

						// Update ExperienceManager with initial progression data FIRST
						ExperienceManager->UpdateCharacterProgression(playerData.characterData.characterId, InitialProgression);

						// THEN initialize experience widget
						UIManager->InitializeExperienceWidget(playerData.characterData.characterId);

						UE_LOG(LogTemp, Warning, TEXT("ExperienceWidget initialized for character %d with initial data: Level %d, XP %d/%d"), 
							playerData.characterData.characterId, 
							InitialProgression.currentLevel,
							InitialProgression.currentExperience,
							InitialProgression.expForNextLevel);
					}
				}
                
			}, 0.5f, false);
		}
	}
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

                if (PlayerHUD->GetDamageCanvas() && PC && MyGameInstance) {
                    // Initialize FCTManager with immediate setup
                    UUIManager* GameUIManager = MyGameInstance->GetUIManager();
                    if (GameUIManager)
                    {
                        GameUIManager->Init(PC, PlayerHUD->GetDamageCanvas(), DamageTextWidgetClass);
                        
                        // Verify FCTManager was created successfully
                        if (UFloatingCombatTextManager* FCTManager = GameUIManager->GetFCTManager())
                        {
                            UE_LOG(LogTemp, Warning, TEXT("CreateHUD: FCTManager successfully initialized and ready"));
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("CreateHUD: FCTManager failed to initialize!"));
                        }

						// Initialize experience data if ExperienceManager is available
						if (UExperienceManager* ExperienceManager = MyGameInstance->GetExperienceManager())
						{
							if (playerData.characterData.characterId > 0)
							{
								// Create initial progression data from current player data
								FPlayerProgressionStruct InitialProgression;
								InitialProgression.characterId = playerData.characterData.characterId;
								InitialProgression.currentLevel = playerData.characterData.characterLevel;
								InitialProgression.currentExperience = playerData.characterData.characterExperiencePoints;
								InitialProgression.totalExperience = playerData.characterData.characterExperiencePoints;
								InitialProgression.expForNextLevel = playerData.characterData.characterExpForNextLevel;
								InitialProgression.expForCurrentLevel = 0; // Will be calculated by server
								InitialProgression.bHasPendingLevelUp = false;
								InitialProgression.pendingLevelGained = 0;

								// Update ExperienceManager with initial progression data
								ExperienceManager->UpdateCharacterProgression(playerData.characterData.characterId, InitialProgression);

								UE_LOG(LogTemp, Warning, TEXT("CreateHUD: Initial experience data set for character %d: Level %d, XP %d/%d"), 
									playerData.characterData.characterId, 
									InitialProgression.currentLevel,
									InitialProgression.currentExperience,
									InitialProgression.expForNextLevel);
							}
						}
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("CreateHUD: UIManager not found in GameInstance"));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("CreateHUD: Missing required components - Canvas: %s, PC: %s, GameInstance: %s"), 
                        PlayerHUD->GetDamageCanvas() ? TEXT("Valid") : TEXT("NULL"),
                        PC ? TEXT("Valid") : TEXT("NULL"),
                        MyGameInstance ? TEXT("Valid") : TEXT("NULL"));
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

            // Update HUD with current and max values
            PlayerHUD->SetHP(playerData.characterData.characterCurrentHealth, MaxHealth);
            PlayerHUD->SetMana(playerData.characterData.characterCurrentMana, MaxMana);
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

void ABasicPlayer::SetPlayerTag(const FString& Tag)
{
    Tags.Add(FName(*Tag));
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
	UpdateExperienceData(); // Update ExperienceManager when level changes
}

// set player experience points
void ABasicPlayer::SetPlayerExpPoints(int32 ExpPoints)
{
    playerData.characterData.characterExperiencePoints = ExpPoints;
	UpdateExperienceData(); // Update ExperienceManager when experience changes
}

// set player next level exp
void ABasicPlayer::SetPlayerNextLevelExp(int32 NextLevelExp)
{
	playerData.characterData.characterExpForNextLevel = NextLevelExp;
	UpdateExperienceData(); // Update ExperienceManager when next level exp changes
}

// Update experience data in ExperienceManager
void ABasicPlayer::UpdateExperienceData()
{
	if (!MyGameInstance || playerData.isOtherClient)
	{
		return; // Only update for local player
	}

	UExperienceManager* ExperienceManager = MyGameInstance->GetExperienceManager();
	if (ExperienceManager && playerData.characterData.characterId > 0)
	{
		// Create updated progression data from current player data
		FPlayerProgressionStruct UpdatedProgression;
		UpdatedProgression.characterId = playerData.characterData.characterId;
		UpdatedProgression.currentLevel = playerData.characterData.characterLevel;
		UpdatedProgression.currentExperience = playerData.characterData.characterExperiencePoints;
		UpdatedProgression.totalExperience = playerData.characterData.characterExperiencePoints;
		UpdatedProgression.expForNextLevel = playerData.characterData.characterExpForNextLevel;
		UpdatedProgression.expForCurrentLevel = 0; // Will be calculated by server
		UpdatedProgression.bHasPendingLevelUp = false;
		UpdatedProgression.pendingLevelGained = 0;

		// Update ExperienceManager with current progression data
		ExperienceManager->UpdateCharacterProgression(playerData.characterData.characterId, UpdatedProgression);

		UE_LOG(LogTemp, Log, TEXT("Updated experience data for character %d: Level %d, XP %d/%d"), 
			playerData.characterData.characterId, 
			UpdatedProgression.currentLevel,
			UpdatedProgression.currentExperience,
			UpdatedProgression.expForNextLevel);
	}
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


void ABasicPlayer::AttackTarget(int32 TargetID, const FString& SkillSlug, int32 TargetTypeId)
{
    if (!MyGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: MyGameInstance not found"));
        return;
    }

    // Get combat system manager from game instance
    UCombatSystemManager* CombatManager = MyGameInstance->GetCombatSystemManager();
    if (!CombatManager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: CombatSystemManager not found"));
        return;
    }

    // Get skill system manager
    USkillSystemManager* SkillManager = MyGameInstance->GetSkillSystemManager();
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: SkillSystemManager not found"));
        return;
    }

    if (TargetID <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot attack: Invalid target ID"));
        return;
    }

    // Check if we can cast the skill
    if (!SkillManager->CanCastSkill(GetActorId_Implementation(), SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot cast skill %s"), *SkillSlug);
        return;
    }

    // Convert TargetTypeId to ECasterType
    ECasterType TargetType = ECasterType::Mob; // Default
    switch (TargetTypeId)
    {
        case 2:
            TargetType = ECasterType::Player;
            break;
        case 3:
            TargetType = ECasterType::Mob;
            break;
        default:
            TargetType = ECasterType::Mob;
            break;
    }

    // Use skill system to cast the skill
    if (SkillManager->CastSkill(GetActorId_Implementation(), TargetID, SkillSlug, TargetType))
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d attacking target ID: %d with skill: %s, target type id: %d"), 
            GetActorId_Implementation(), TargetID, *SkillSlug, TargetTypeId);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to cast skill %s on target %d"), *SkillSlug, TargetID);
    }
}

// Called when the actor is being destroyed
void ABasicPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister from combat system
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
        {
            // Безопасно отписываемся только если объект ещё валиден
            if (IsValid(this) && GetActorId_Implementation() > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->UnregisterCombatable(CombatableInterface);
                UE_LOG(LogTemp, Log, TEXT("Player %d unregistered from combat system"), GetActorId_Implementation());
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

// ICombatable interface implementations
int32 ABasicPlayer::GetMaxHealth_Implementation() const
{
    if (const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health")))
    {
        return HealthAttr->attributeValue;
    }
    return 100; // Default value
}

int32 ABasicPlayer::GetMaxMana_Implementation() const
{
    if (const FAttributeDataStruct* ManaAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana")))
    {
        return ManaAttr->attributeValue;
    }
    return 100; // Default value
}

void ABasicPlayer::SetDead_Implementation(bool bNewDead)
{
    playerData.characterData.bIsDead = bNewDead;
    
    if (bNewDead)
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d has died"), GetActorId_Implementation());
        OnDeath_Implementation();
    }
}

void ABasicPlayer::OnDeath_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("Player %s died"), *playerData.characterData.characterName);
    
    // Clear target when dying
    ClearTarget_Implementation();
    
    // Handle player death logic here
    // For example: disable movement, play death animation, etc.
}

void ABasicPlayer::SetTarget_Implementation(int32 TargetId, ECasterType TargetType)
{
    CurrentTargetId = TargetId;
    CurrentTargetType = TargetType;
    
    UE_LOG(LogTemp, Log, TEXT("Player %d set target: %d (%s)"), 
        GetActorId_Implementation(), TargetId, *UEnum::GetValueAsString(TargetType));
}

void ABasicPlayer::ClearTarget_Implementation()
{
    CurrentTargetId = 0;
    CurrentTargetType = ECasterType::None;
    
    UE_LOG(LogTemp, Log, TEXT("Player %d cleared target"), GetActorId_Implementation());
}

void ABasicPlayer::PlaySkillAnimation_Implementation(const FString& AnimationName, float Duration)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d playing skill animation: %s (Duration: %.1f)"), 
        GetActorId_Implementation(), *AnimationName, Duration);
    
    // Play animation logic here
    // You might want to trigger animation montages or other visual effects
}

void ABasicPlayer::ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d taking %d %s damage (Critical: %s, School: %s)"), 
        GetActorId_Implementation(), Damage, bIsCritical ? TEXT("CRITICAL") : TEXT("normal"),
        bIsCritical ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(School));
    
}

void ABasicPlayer::ShowHealingEffect_Implementation(int32 Healing)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d healed for %d"), GetActorId_Implementation(), Healing);
    
}

void ABasicPlayer::ShowBuffEffect_Implementation(const FAppliedEffectData& Effect)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d received %s effect: %s (Value: %d, Duration: %.1f)"), 
        GetActorId_Implementation(), *Effect.effectType, *Effect.effectName, Effect.value, Effect.duration);
    
    // Handle buff/debuff visual effects here
    // You might want to show icons, particle effects, etc.
}

// Add these method implementations at the end of the file, before the closing brace

void ABasicPlayer::UpdatePlayerStats(const FPlayerStatsUpdateStruct& StatsUpdate)
{
	// Validate the stats update
	if (!PlayerAttributeParser::ValidateStatsData(StatsUpdate))
	{
		UE_LOG(LogTemp, Error, TEXT("BasicPlayer: Invalid stats update data received"));
		return;
	}

	// Check if this update is for this player
	if (StatsUpdate.characterId != playerData.characterData.characterId)
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Stats update for different character (Expected: %d, Received: %d)"), 
			playerData.characterData.characterId, StatsUpdate.characterId);
		return;
	}

	// Update character data
	PlayerAttributeParser::UpdateCharacterDataFromStatsUpdate(playerData.characterData, StatsUpdate);

	UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Updated stats for character %d - Level: %d, HP: %d/%d, MP: %d/%d"),
		StatsUpdate.characterId, StatsUpdate.level,
		StatsUpdate.healthCurrent, StatsUpdate.healthMax,
		StatsUpdate.manaCurrent, StatsUpdate.manaMax);
}

void ABasicPlayer::ProcessStatsUpdate(const FPlayerStatsUpdateStruct& StatsUpdate)
{
	// Update the player data
	UpdatePlayerStats(StatsUpdate);
	
	// Refresh the UI to reflect new stats
	RefreshHUD();
	
	// Update experience data if level changed
	if (StatsUpdate.level != playerData.characterData.characterLevel)
	{
		UpdateExperienceData();
	}
	
	UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Processed stats update and refreshed UI"));
}

void ABasicPlayer::RefreshHUD()
{
	if (!PlayerHUD)
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Cannot refresh HUD - PlayerHUD is null"));
		return;
	}

	// Get max health and mana from attributes
	float MaxHealth = 100.0f; // Default value
	float MaxMana = 100.0f;   // Default value

	// Try to get max values from character attributes
	if (const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health")))
	{
		MaxHealth = static_cast<float>(HealthAttr->attributeValue);
	}

	if (const FAttributeDataStruct* ManaAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana")))
	{
		MaxMana = static_cast<float>(ManaAttr->attributeValue);
	}

	// Update HUD with current values
	PlayerHUD->SetHP(static_cast<float>(playerData.characterData.characterCurrentHealth), MaxHealth);
	PlayerHUD->SetMana(static_cast<float>(playerData.characterData.characterCurrentMana), MaxMana);

	UE_LOG(LogTemp, VeryVerbose, TEXT("BasicPlayer: HUD refreshed - HP: %.0f/%.0f, MP: %.0f/%.0f"),
		static_cast<float>(playerData.characterData.characterCurrentHealth), MaxHealth,
		static_cast<float>(playerData.characterData.characterCurrentMana), MaxMana);
}

