// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "Services/TimeSyncService.h"
#include "MOBMovementComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UMOBMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOBMovementComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Configuration -------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxAcceleration = 800.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MinMoveSpeed = 580.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SnapDistance = 10.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float TeleportThreshold = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundTraceHeight = 200.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundTraceDepth = 400.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundInterpSpeedDown = 15.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundInterpSpeedUp = 8.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
	float MoveRotationSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
	float AttackRotationSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Debug")
	bool bDebugGroundAdjustment = false;

	// ---- Public API ----------------------------------------------------------

	void OnReceiveServerPacket(const FPositionDataStruct& MOBPosition);

	FVector AdjustToGround(const FVector& Location, float DeltaTime, float Priority = 1.0f);
	void SnapToGround();

	void ProcessMovement(float DeltaTime);
	void HandleCloseRangeMovement(const FVector& CurrentLocation, float DeltaTime);

	FVector CalculateMovementPosition(
		const FVector& CurrentLocation,
		const FVector& TargetXY,
		const FVector& CurrentXY,
		float HorizontalDist,
		float DeltaTime);

	void HandleRotation(const FVector& MoveVector, float DeltaTime);

	bool IsMoving() const { return bIsMoving; }
	float GetCurrentSpeed() const { return CurrentInterpSpeed; }
	bool IsFleeing() const { return CombatState == 7; }
	bool IsInCombatState() const { return CombatState >= 1 && CombatState <= 4; }

	// ---- Target tracking -----------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Target Tracking")
	bool bEnableTargetTracking = true;

	UPROPERTY(EditAnywhere, Category = "Target Tracking")
	float TargetTrackingSpeed = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Target Tracking")
	float MinAngleThreshold = 5.0f;

	void SetTargetId(int32 NewTargetId);
	void SetTargetType(const FString& NewTargetType);
	void ClearTarget();
	int32 GetTargetId() const { return CurrentTargetId; }

	// ---- State setters -------------------------------------------------------

	void SetCombatState(int32 NewState) { CombatState = NewState; }
	int32 GetCombatState() const { return CombatState; }

	void SetTimeSyncService(class UTimeSyncService* InService) { TimeSyncServiceRef = InService; }

	void OnReceiveMovePacket(const FMobMoveEntryStruct& MoveEntry, int64 ServerSendMs, int64 ClientRecvMs);

private:
	// ---- Dead-reckoning / interpolation state --------------------------------

	FVector  PrevServerPos;
	FVector  TargetServerPos;
	FVector  ServerVelocity;
	FVector  Waypoint;
	bool     bHasWaypoint = false;
	FRotator PrevServerRot;
	FRotator TargetServerRot;

	float    ServerSpeed = 0.f;

	int64    LastStepTimestampMs = 0;
	int64    LastPacketClientRecvMs = 0;
	float    LastMovePacketTime = 0.f;
	bool     bHasReceivedPacket = false;

	float    CurrentInterpSpeed = 0.f;
	bool     bIsMoving = false;

	float    TimeSinceLastGroundCheck = 0.0f;

	FVector  SmoothedGroundZ;
	float    CachedGroundZ = 0.f;
	bool     bHasCachedGroundZ = false;

	int32    CombatState = 0;

	TWeakObjectPtr<class UTimeSyncService> TimeSyncServiceRef;

	void UpdateMovingState(bool bNewIsMoving);

	FVector ComputeDeadReckonedTarget(float DeltaTime) const;

	FVector TraceGround(const FVector& Location) const;

	// ---- Target tracking state -----------------------------------------------

	int32   CurrentTargetId = 0;
	FString CurrentTargetType;
	float   TimeSinceLastTargetUpdate = 0.0f;

	void    UpdateTargetTracking(float DeltaTime);
	AActor* FindTargetActor(int32 TargetId, FString TargetType = "");
	void    RotateTowardsTarget(AActor* TargetActor, float DeltaTime);
};