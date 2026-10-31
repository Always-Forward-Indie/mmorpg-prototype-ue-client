#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Gameplay/Teleport/TeleportParser.h"
#include "TeleportManager.generated.h"

class ABasicPlayer;
class UMyGameInstance;
class UNetworkManager;

/**
 * Server-authoritative teleport / respawn cover-transition manager.
 *
 * Single owner of the client teleport flow (AAA cover transition):
 *   1. Freeze — lock movement input, cancel actions (auto-attack, target,
 *      cast bar, pending interactions), zero velocity, flush pending combat.
 *   2. Fade out — PlayerCameraManager camera fade to black (0.22s).
 *   3. Apply — snap to the authoritative target at fade peak (invisible),
 *      ground-snap, controller yaw sync, camera-boom snap (lag off for 1 tick).
 *   4. Reveal — extended hold for far teleports (streaming), fade back in,
 *      unlock input, send teleportAck (requestIdEcho-bound).
 *
 * Remote players never fade: their snaps apply immediately through the same
 * ABasicPlayer::SnapToAuthoritativePosition single source of truth.
 * Respawn reuses the same flow; vitals land before revive, the death screen
 * hides at fade peak (under black).
 *
 * Owned by UMyGameInstance (created in Boot, initialized in InitSystemsA).
 * WorldContext follows the PreLoadMap/PostLoadMap null/refresh discipline.
 */
UCLASS()
class PROTOTYPING_API UTeleportManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);
	void SetWorldContext(UWorld* World);

	/** Main entry: server unicast (respawnResult / playerTeleported / skill teleport). */
	void ApplyServerTeleport(const FTeleportTargetData& Target, bool bIsRespawn);

	/** Offline entry: DevMode console / bot scenario (no ack, no vitals). */
	void ApplyLocalTeleport(ABasicPlayer* Player, const FVector& Dest, float Yaw, const FString& Reason);

	/** True while a local cover transition (fade..reveal) is in flight. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Teleport")
	bool IsTransitionActive() const { return bTransitionActive; }

	/** Correction gate: false when a positionCorrection is stale history
	 *  racing a cover transition (active cover, or <1s after reveal while
	 *  the correction points at the pre-teleport spot). Genuine drift
	 *  (small nudges, remote pawns) always applies. */
	bool ShouldApplyCorrection(int32 CharacterId, const FVector& CorrPos) const;

	/** Abort timers on world teardown so no callback fires into a dead world. */
	void CancelActiveTransition();

	/** Player lookup for packet handling (null when unknown). */
	ABasicPlayer* FindPlayerByCharacterId(int32 CharacterId) const;

	/** Compat: resolve a missing body characterId via the connected-player map. */
	int32 ResolveCharacterId(int32 BodyCharacterId, int32 ClientId) const;

private:
	/** Cover entry for the locally-controlled pawn (fade out + lock). */
	void BeginCoverTransition(ABasicPlayer* Player, const FString& Reason);
	/** Snap + vitals + revive at fade peak (runs under black). */
	void ExecuteCoveredSnap();
	/** Fade in + unlock + ack after the hold. */
	void ExecuteReveal();
	/** Safety: force-unlock if reveal timers never fired. */
	void ExecuteSafetyUnlock();

	void SendTeleportAck(int32 CharacterId, const FVector& AppliedPos, float AppliedYaw, const FString& RequestIdEcho, const FString& Reason);
	void ClearTimers();

	ABasicPlayer* FindPlayer(int32 CharacterId) const;

	UPROPERTY()
	UWorld* worldContext = nullptr;

	UPROPERTY()
	UNetworkManager* networkManager = nullptr;

	UPROPERTY()
	UMyGameInstance* gameInstance = nullptr;

	bool bTransitionActive = false;

	/** Monotonic seconds of the last reveal (cover exit). Guards the
	 *  post-reveal tail against stale positionCorrections. */
	double LastRevealTimeSec = -1e9;

	UPROPERTY()
	ABasicPlayer* ActivePlayer = nullptr;

	FTeleportTargetData ActiveTarget;
	bool bActiveIsRespawn = false;
	FString ActiveReason;

	FTimerHandle SnapTimerHandle;
	FTimerHandle RevealTimerHandle;
	FTimerHandle SafetyTimerHandle;
};
