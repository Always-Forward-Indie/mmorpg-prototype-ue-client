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
	float TeleportThreshold = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundTraceHeight = 200.f;

	// How far below the current actor Z the ground trace extends.
	// Must be large enough to reach terrain when the mob is elevated from stepping
	// on props/fences.  400 was too shallow for objects taller than ~300 units.
	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundTraceDepth = 800.f;

	// When descending, if the center-only trace finds ground this many units below
	// the multi-probe median, the center result is trusted over the median.
	// This lets the mob snap down quickly after stepping off a fence/ledge even
	// when the surrounding offset probes still "see" the elevated surface.
	UPROPERTY(EditAnywhere, Category = "Movement")
	float CenterPriorityThreshold = 30.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundInterpSpeedDown = 15.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float GroundInterpSpeedUp = 8.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
	float MoveRotationSpeed = 12.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
	float AttackRotationSpeed = 8.f;

	// Constant rotation rate (deg/s) used during patrol movement.
	// Lower than combat rotation to avoid jarring snaps when patrol direction changes.
	UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
	float PatrolRotationRate = 270.f;

	// Angle (degrees) below which the mob stops rotating in place and begins walking.
	UPROPERTY(EditAnywhere, Category = "Movement|Rotation")
	float PatrolTurnStartAngle = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Debug")
	bool bDebugGroundAdjustment = false;

	// ---- Movement tuning -----------------------------------------------------

	// Duration (sec) of the blend from client position toward the server-authoritative
	// position when a new combat packet arrives. Controls how quickly the client
	// corrects positional drift. Shorter = snappier, longer = smoother.
	UPROPERTY(EditAnywhere, Category = "Movement|Correction")
	float BlendToServerTime = 0.1f;

	// Duration (sec) of the positional blend on combat-state transitions
	// (e.g. patrol->chase). Per protocol: 100-150 ms.
	UPROPERTY(EditAnywhere, Category = "Movement|Correction")
	float StateTransitionBlendTime = 0.12f;

	// Max time (sec) to extrapolate beyond the last packet before freezing.
	// Protocol: cap at 200ms for combat states.
	UPROPERTY(EditAnywhere, Category = "Movement|Correction")
	float ExtrapolationMaxTime = 0.2f;

	// Distance (units) above which a patrol-to-patrol packet triggers a blend
	// instead of relying on continuous correction alone.  Covers the case where
	// the server steps 200-450 units between packets (patrol step timing).
	UPROPERTY(EditAnywhere, Category = "Movement|Correction")
	float PatrolSnapThreshold = 40.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Blend")
	float StoppedSpeedThreshold = 5.f;

	// ---- Public API ----------------------------------------------------------

	void OnReceiveServerPacket(const FPositionDataStruct& MOBPosition);

	FVector AdjustToGround(const FVector& Location, float DeltaTime, float Priority = 1.0f);
	void SnapToGround();

	void ProcessMovement(float DeltaTime);

	void HandleRotation(const FVector& MoveDir, float DeltaTime);

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

	// Called from spawn to seed the initial movement state from spawn-data velocity
	void InitializeFromSpawnData(const FVector& SpawnPos, const FVector& VelocityDir, float Speed, int32 InCombatState);

	// Permanently stops all movement processing (called on mob death).
	// After this call the component will not move the actor even if packets arrive.
	void FreezeMob();

private:
	// ---- Server state (latest authoritative packet) --------------------------
	FVector  LatestServerPos   = FVector::ZeroVector;  // XY from server, Z from actor
	FVector  LatestServerDir   = FVector::ZeroVector;  // normalised XY direction
	float    LatestServerSpeed = 0.f;                  // units/sec

	// ---- Patrol state --------------------------------------------------------
	FVector  Waypoint              = FVector::ZeroVector;
	bool     bHasWaypoint          = false;
	float    PatrolSpeed           = 0.f;
	bool     bPatrolReachedWaypoint = false;

	// ---- Server-position blend -----------------------------------------------
	// Instead of correction offsets, we track the visual position as a blend
	// between where the client currently is and where the server says the mob is.
	FVector  BlendFromPos      = FVector::ZeroVector;  // client pos at packet arrival
	FVector  BlendToPos        = FVector::ZeroVector;  // server pos from packet
	float    BlendElapsed      = 0.f;
	float    BlendDuration     = 0.1f;
	bool     bBlendActive      = false;

	// Set to true by FreezeMob() — disables all tick processing after mob death.
	bool     bFrozen                = false;

	// ---- Packet timing -------------------------------------------------------
	int64    LastStepTimestampMs    = 0;
	int64    LastPacketClientRecvMs = 0;
	float    LastMovePacketTime     = 0.f;
	bool     bHasReceivedPacket     = false;
	float    TimeSinceLastPacket    = 0.f;
	float    TimeSinceLastGroundCheck = 0.0f;

	// ---- Animation / display state -------------------------------------------
	float    CurrentInterpSpeed  = 0.f;
	bool     bIsMoving           = false;

	// Smoothed facing direction to avoid per-tick jitter
	FVector  SmoothedFacingDir   = FVector::ForwardVector;

	// Smoothed movement direction for combat dead-reckoning.
	// Dampens deflection-avoidance angle changes across packets.
	FVector  DeadReckonDir       = FVector::ForwardVector;

	// Previous frame XY position — used to derive actual movement direction for rotation
	FVector  PrevFramePos        = FVector::ZeroVector;

	// Hysteresis thresholds for bIsMoving
	float    MovingStartThreshold = 20.f;
	float    MovingStopThreshold  = 8.f;

	int32    CombatState = 0;
	int32    PrevCombatState = 0;

	TWeakObjectPtr<class UTimeSyncService> TimeSyncServiceRef;

	void    UpdateMovingState(bool bNewIsMoving);
	void    ProcessPatrolMovement(float DeltaTime);
	void    ProcessCombatMovement(float DeltaTime);
	FVector TraceGround(const FVector& Location) const;

	// ---- Target tracking state -----------------------------------------------
	int32   CurrentTargetId = 0;
	FString CurrentTargetType;
	float   TimeSinceLastTargetUpdate = 0.0f;

	void    UpdateTargetTracking(float DeltaTime);
	AActor* FindTargetActor(int32 TargetId, FString TargetType = "");
	void    RotateTowardsTarget(AActor* TargetActor, float DeltaTime);
};