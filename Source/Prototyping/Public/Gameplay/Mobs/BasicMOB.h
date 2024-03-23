// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "Data/DataStructs.h"
#include "BasicMOB.generated.h"

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

public:
	// Sets default values for this character's properties
	ABasicMOB();

	// Event declaration
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMOBDataUpdated);

	// Event variable
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnMOBDataUpdated MOBDataUpdated;


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
	void SetMOBPosition(const FPositionDataStruct& MOBPosition);

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


	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobNameText(UTextRenderComponent* MobNameTextComponent, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobLevelText(UTextRenderComponent* MobLevelTextComponent, const FString& Level);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBTag(const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void MOBTextFaceToCamera(UTextRenderComponent* MobTextComponent);

	// MOB Movement
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void UpdatePosition();
	float CalculateInterpolationSpeed(float MovementSpeed);
	void InterpolateMovement(float DeltaTime, float InterpolationSpeed);
	FDateTime StringToTimestamp(const FString& DateTimeString);


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
