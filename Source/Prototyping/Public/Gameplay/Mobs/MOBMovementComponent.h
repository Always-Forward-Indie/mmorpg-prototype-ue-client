// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "MOBMovementComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UMOBMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMOBMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Movement configuration
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxAcceleration = 800.f; // Maximum acceleration (units/sec^2)

	// Minimum speed (units/sec) to maintain for small distances
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MinMoveSpeed = 580.f;

	// Distance (units) for direct snapping without interpolation
	UPROPERTY(EditAnywhere, Category = "Movement")
	float SnapDistance = 10.f;

	// Debug visualization for ground adjustment
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Debug")
	bool bDebugGroundAdjustment = true;

	// Set new position target from server packet
	void OnReceiveServerPacket(const FPositionDataStruct& MOBPosition);

	// Adjust movement position to ground with priority control
	FVector AdjustToGround(const FVector& Location, float DeltaTime, float Priority = 1.0f);

	void SnapToGround();

	// Process movement interpolation 
	void ProcessMovement(float DeltaTime);

	// Handles movement when close to target position
	void HandleCloseRangeMovement(const FVector& CurrentLocation, float DeltaTime);

	// Calculates the next position during movement
	FVector CalculateMovementPosition(
		const FVector& CurrentLocation,
		const FVector& TargetXY,
		const FVector& CurrentXY,
		float HorizontalDist,
		float DeltaTime);

	// Handles character rotation during movement
	void HandleRotation(const FVector& MoveVector, float DeltaTime);

	// Is the mob currently moving
	bool IsMoving() const { return bIsMoving; }

	// Target tracking functionality
	UPROPERTY(EditAnywhere, Category = "Target Tracking")
	bool bEnableTargetTracking = true;

	UPROPERTY(EditAnywhere, Category = "Target Tracking")
	float TargetTrackingSpeed = 360.0f; // Degrees per second

	UPROPERTY(EditAnywhere, Category = "Target Tracking")
	float MinAngleThreshold = 5.0f; // Don't rotate if already facing target within this angle

	// Set target for tracking
	void SetTargetId(int32 NewTargetId);

	// Set target type (e.g., "Player", "Mob")
	void SetTargetType(const FString& NewTargetType);

	// Clear current target
	void ClearTarget();

	// Get current target ID
	int32 GetTargetId() const { return CurrentTargetId; }

private:
	// Internal movement state
	FVector PrevServerPos;
	FVector TargetServerPos;
	FVector ServerVelocity;
	FRotator PrevServerRot;
	FRotator TargetServerRot;
	float LastMovePacketTime;
	bool bHasVelocity;
	float CurrentInterpSpeed = 0.f;
	bool bIsMoving = false;

	float TimeSinceLastGroundCheck = 0.0f;

	// Function to notify owner that movement state changed
	void UpdateMovingState(bool bNewIsMoving);

	// Target tracking state
	int32 CurrentTargetId = 0;
	// Current Target Type (e.g., "Player", "Mob")
	FString CurrentTargetType;
	// Time since last target update
	float TimeSinceLastTargetUpdate = 0.0f;



	// Target tracking methods
	void UpdateTargetTracking(float DeltaTime);
	AActor* FindTargetActor(int32 TargetId, FString TargetType = "");
	void RotateTowardsTarget(AActor* TargetActor, float DeltaTime);
};