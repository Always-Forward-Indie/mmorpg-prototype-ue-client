// Login Level Setup Actor — place one instance in the Login Level.
// GameInstance reads camera and character-spawn transforms from its named
// child components at runtime, eliminating the need to enter raw coordinates.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LoginLevelSetupActor.generated.h"

class UArrowComponent;

/**
 * Place exactly one ALoginLevelSetupActor in the Login Level.
 * Position each arrow component visually in the viewport.
 * UMyGameInstance::OnLoginLevelLoaded reads transforms from these components
 * and populates PodiumSpawnLocations, PodiumCameraLocation, etc. automatically.
 *
 * Component colour guide:
 *   Blue   — login overview camera (initial view)
 *   Cyan   — character-select camera (podium overview)
 *   Green  — character-create camera (close-up)
 *   Yellow — podium character slots (0–3, left to right)
 *   Orange — character-create slot
 */
UCLASS(Blueprintable, BlueprintType, HideCategories = (Rendering, Replication, Input, LOD, Actor))
class PROTOTYPING_API ALoginLevelSetupActor : public AActor
{
	GENERATED_BODY()

public:
	ALoginLevelSetupActor();

	// ─── Camera spots ─────────────────────────────────────────────────────

	/** Login overview camera (initial view when the level loads). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login Level|Cameras")
	UArrowComponent* LoginCameraSpot;

	/** Camera position for the character-select podium overview. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login Level|Cameras")
	UArrowComponent* SelectCameraSpot;

	/** Camera position for the character-create close-up. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login Level|Cameras")
	UArrowComponent* CreateCameraSpot;

	// ─── Character slots ──────────────────────────────────────────────────

	/** Podium spawn spots for character-select previews (up to 4, indices 0–3). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login Level|Characters")
	TArray<UArrowComponent*> PodiumSlots;

	/** Spawn spot for the single character-create preview. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login Level|Characters")
	UArrowComponent* CreateSlot;

	/**
	 * Position the selected character steps forward to — between the podium and the camera.
	 * The character lerps here when selected and back to its slot when deselected.
	 * Colour: Red.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Login Level|Characters")
	UArrowComponent* SelectedCharacterSlot;
};
