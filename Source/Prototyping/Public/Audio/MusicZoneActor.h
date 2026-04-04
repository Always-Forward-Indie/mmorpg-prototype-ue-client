// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MusicZoneActor.generated.h"

class UBoxComponent;

/**
 * AMusicZoneActor
 *
 * Place this actor in any level to define a music zone.
 * When the local player's pawn enters the box, the AudioManager switches
 * to the specified playlist.  When the pawn leaves and no other
 * MusicZoneActor is active, music is stopped.
 *
 * Designer workflow:
 *   1. Drag BP_MusicZoneActor into the level.
 *   2. Scale the BoxComponent to cover the zone area.
 *   3. Set PlaylistId to a playlist defined in the GameInstance AudioManager.
 *   4. Optionally set FadeOutTimeOverride > 0 to customise exit fade.
 *
 * For the Login level (no player pawn):
 *   - Set PlaylistId and bTriggerWithoutPawn = true.
 *     Music starts as soon as BeginPlay fires (no overlap needed).
 *   - The Login camera actor is typically inside this zone.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API AMusicZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AMusicZoneActor();

	/** Trigger volume — resize this to define the zone boundary. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Music Zone")
	UBoxComponent* TriggerBox = nullptr;

	/** The FMusicPlaylist::PlaylistId to play when the player enters this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Zone")
	FString PlaylistId;

	/** Override the fade-out time when leaving this zone (0 = use playlist default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Zone", meta = (ClampMin = "0.0"))
	float FadeOutTimeOverride = 0.0f;

	/** If true, music restarts from the beginning even if this playlist is already active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Zone")
	bool bForceRestartOnEnter = false;

	/** If true, start the playlist immediately at BeginPlay without waiting for a pawn overlap.
	 *  Use this on the Login level where there is no player pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Zone")
	bool bTriggerWithoutPawn = false;

	/** Called by GameInstance (NotifyPlayerSpawned path) after the local player pawn is
	 *  present in the world so the initial overlap check can fire immediately instead of
	 *  waiting for the next 0.25 s timer tick. */
	void OnPlayerSpawned();

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle OverlapCheckTimerHandle;

	// True while the local pawn is considered inside this zone.
	// Guards against spurious EndOverlap events that UE generates during
	// Actor spawn / Possess / capsule resize — these must not stop the music.
	bool bIsPlayerInside = false;

	void CheckInitialOverlap();

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
