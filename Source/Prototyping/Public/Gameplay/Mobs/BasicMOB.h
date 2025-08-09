// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "Data/DataStructs.h"
#include <Gameplay/UI/MOBHeadInfo.h>
#include "Components/CapsuleComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Components/AudioComponent.h"
#include <Gameplay/UI/W_MOBHeadInfoWidget.h>
#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "BasicMOB.generated.h"

// Event declaration
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMOBDataUpdated);

/**
 *
 */
UCLASS()
class PROTOTYPING_API ABasicMOB : public ACharacter
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB Data", meta = (AllowPrivateAccess = "true"))
	FMOBStruct MOBData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB Data", meta = (AllowPrivateAccess = "true"))
	float LastUpdateTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB Data", meta = (AllowPrivateAccess = "true"))
	FString lastTimestamp = "";

	
	FVector LastReceivedPosition = FVector::ZeroVector;
	FVector TargetReceivedPosition = FVector::ZeroVector;

	float TimeSinceLastPositionUpdate = 0.0f;
	float LastServerTimestamp = 0.f;
	float TargetServerTimestamp = 0.f;
	float TimeOffset = 0.f; // offset = ServerTimestamp - GetWorld()->GetTimeSeconds()

	bool bInitialSyncDone = false;

	FVector PrevServerPos;
	FVector TargetServerPos;
	FVector ServerVelocity;
	FRotator PrevServerRot;
	FRotator TargetServerRot;
	float   LastMovePacketTime;
	bool    bHasVelocity;
	float ExpectedNextPacketTime = 0.0f;

	float CurrentInterpSpeed = 0.f;


public:
	// Sets default values for this character's properties
	ABasicMOB();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	class UMOBMovementComponent* MOBMovementComponent;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowDebugSpheres = false;

	void OnReceiveServerPacket(const FPositionDataStruct& MOBPosition);

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxAcceleration = 800.f; // Максимальное ускорение (юнитов/с^2)

	// Минимальная скорость (юнитов/с), чтобы не замедляться слишком сильно на малых отрезках
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MinMoveSpeed = 580.f;

	// Растояние (юнитов), при котором мы «за snapping» к точке без интерполяции
	UPROPERTY(EditAnywhere, Category = "Movement")
	float SnapDistance = 10.f;

	// Helper function to adjust mob position to ground
	// Helper function to adjust mob position to ground with priority control
	FVector AdjustToGround(const FVector& Location, float DeltaTime, float Priority = 1.0f);

	// Enables debug visualization for ground adjustment
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Debug")
	bool bDebugGroundAdjustment = false;

	// Event variable
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnMOBDataUpdated MOBDataUpdated;


	// Add this with your other timer handles
	FTimerHandle DamageFlagResetTimer;

	// Add this with your other configurable properties
	UPROPERTY(EditAnywhere, Category = "Damage")
	float DamageFlagResetTime = 0.5f; // Time in seconds before damage flag


	UW_MOBHeadInfoWidget* HeadWidget;

	// Constant movement speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float MoveSpeed = 200.0f; // Adjust as needed

	// Constant rotation speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float RotationSpeed = 1000.0f; // Adjust as needed

	// Interpolation speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float interpolationSpeedFactor = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float maxInterpolationSpeed = 1200.0f;

	UFUNCTION(BlueprintCallable, Category = "MOB")
	FMOBStruct GetMOBData() const;

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBData(const FMOBStruct& Data);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBId(const int& MOBId);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBUId(const FString& MOBUId);
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBZoneId(const int& MOBZoneId);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBName(const FString& MOBName);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBRace(const FString& MOBRace);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBLevel(const int& MOBLevel);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBCurrentHealth(const int& MOBCurrentHealth);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBCurrentMana(const int& MOBCurrentMana);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBPosition(const FPositionDataStruct& MOBPosition, const FString& PacketTimestamp);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBAttributes(const FAttributesDataStruct& MOBAttributes);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBAttribute(const FString& AttributeSlug, const FAttributeDataStruct& MOBAttribute);

	// set mob is dead
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBIsDead(const bool& MOBIsDead);

	// set mob is aggressive
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBIsAggressive(const bool& MOBIsAggressive);

	//set mob is moving
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBIsMoving(const bool& MOBIsMoving);
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobIsDamaged(const bool& bIsDamaged);

	// set last timestamp
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetLastTimestamp(const FString& Timestamp);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	//get mob name
	FString GetMobName() const;

	// get mob unique id
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FString GetMOBUId() const;

	// get mob id
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBId() const;

	// get mob race	
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FString GetMOBRace() const;

	// get mob level
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBLevel() const;

	// get mob current health
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBCurrentHealth() const;

	// get mob current mana
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBCurrentMana() const;

	// get mob position
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FVector GetMOBPosition() const;

	// get mob attributes
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FAttributesDataStruct GetMOBAttributes() const;

	// get mob is dead
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsDead() const;

	// get mob is aggressive
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsAggressive() const;

	//get mob is moving
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsMoving() const;

	// get mob is damaged
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsDamaged() const;


	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobNameText(UTextRenderComponent* MobNameTextComponent, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobLevelText(UTextRenderComponent* MobLevelTextComponent, const FString& Level);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBTag(const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void MOBTextFaceToCamera(UTextRenderComponent* MobTextComponent);

	// Force update UI
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void ForceUpdateUI();

	// Initialize UI with delay
	UFUNCTION()
	void InitializeUIDelayed();

	void Die();

	public:
		UPROPERTY(EditAnywhere, Category = "UI")
		UMOBHeadInfo* MobHeadInfo;

		// In your BasicMOB.h file add these with more appropriate values
		UPROPERTY(EditAnywhere, Category = "UI")
		float MinSize = 80.0f; // Smaller size when far away

		UPROPERTY(EditAnywhere, Category = "UI")
		float MaxSize = 250.0f; // Original size when close

		UPROPERTY(EditAnywhere, Category = "UI")
		float MinDistance = 700.0f; // Distance at which UI is full size

		UPROPERTY(EditAnywhere, Category = "UI")
		float MaxDistance = 2500.0f; // Distance at which UI is minimum size

		UPROPERTY(EditAnywhere, Category = "UI")
		float widgetScaleFactor = 1.0f; // Scale factor for the widget

		UPROPERTY(EditAnywhere, Category = "UI")
		float CurrentWidgetScale = 1.0f;

		UPROPERTY(EditAnywhere, Category = "UI")
		float InterpSpeedFactor = 5.0f;


		int LastHealth = 0;
		int LastMana = 0;
		bool bUIInitialized = false;


	protected:
		// Called when the game starts or when spawned
		virtual void BeginPlay() override;

	public:	
		// Called every frame
		virtual void Tick(float DeltaTime) override;

		void UpdateWidgetScale(float DeltaTime);

		void UpdateWidgetPosition();

		// Called to bind functionality to input
		virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

		void SetupMobVisual(FName MobSlug);
		void SetupMobAudio(FName MobSlug);

		void PlayRandomIdleSound();

		void PlaySoundByName(FName SoundName);

		void PlayWalkRandomSound();

		void PlayRunRandomSound();

		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOBs Config")
		UDataTable* MobDefinitionTable;

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
		UAudioComponent* AudioComponentMain;

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
		UAudioComponent* AudioComponentSecond;

		// Звуки
		UPROPERTY()
		TMap<FName, USoundBase*> SoundMap;

		UPROPERTY()
		TArray<USoundBase*> IdleSounds;

		UPROPERTY()
		TArray<USoundBase*> WalkSounds;

		UPROPERTY()
		TArray<USoundBase*> RunSounds;

		FTimerHandle IdleSoundTimer;
};
