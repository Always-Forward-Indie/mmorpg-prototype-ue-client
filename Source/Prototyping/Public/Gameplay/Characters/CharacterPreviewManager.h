// Character Preview Manager — Spawns 3D character previews on the login
// screen podium for character select and character create flows.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Authentication/LoginFlowTypes.h"
#include "CharacterPreviewManager.generated.h"

class ABasicPlayer;
class UMyGameInstance;
class UDataTable;
class UNameplateCanvasWidget;

UCLASS(BlueprintType)
class PROTOTYPING_API UCharacterPreviewManager : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize with GameInstance reference. Call once when the login level loads. */
	void Initialize(UMyGameInstance* GI);

	/** Destroy all preview actors and reset state. Call when leaving the login level. */
	void Cleanup();

	// ─── Character Select ────────────────────────────────────────────────────

	/** Spawn preview actors for each character in the list on the podium. */
	void SpawnCharacterPreviews(const TArray<FLoginCharacterEntry>& Characters);

	/**
	 * Highlight a character by index — shows the selection ring and scales down others.
	 * Index -1 clears all highlights.
	 */
	void HighlightCharacter(int32 Index);

	/**
	 * Move the actor at the given podium index to the SelectedCharacterSlot focus position.
	 * Called automatically by HighlightCharacter; exposed for external use if needed.
	 */
	void MoveCharacterToFocusSlot(int32 Index);

	/**
	 * Return the actor at the given podium index back to its original podium position.
	 * Called automatically by HighlightCharacter when a previous selection is cleared.
	 */
	void RestoreCharacterToSlot(int32 Index);

	/** Walk speed (cm/s) used for the character-select podium walk animations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Preview")
	float PreviewWalkSpeed = 200.f;

	/**
	 * Tick smooth movement of preview characters toward their target slots.
	 * Must be called every frame from LoginFlowWidget::NativeTick while the
	 * Character Select panel is active.
	 */
	void TickCharacterMovements(float DeltaTime);

	/** Destroy all select-mode preview actors. */
	void ClearSelectPreviews();

	/** Returns number of currently spawned select previews. */
	int32 GetSelectPreviewCount() const { return SelectPreviewActors.Num(); }

	/**
	 * Returns the index of a preview actor in the podium array, or -1 if not found.
	 * Used for click-to-select hit testing.
	 */
	int32 GetPreviewActorIndex(const ABasicPlayer* Actor) const;

	// ─── Character Create ────────────────────────────────────────────────────

	/** Spawn or update a single create-mode preview actor with the given combo. */
	void UpdateCreatePreview(const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName);

	/** Destroy the create-mode preview actor. */
	void ClearCreatePreview();

	// ─── Camera ──────────────────────────────────────────────────────────────

	/** Blend camera to the character select view (podium overview). */
	void BlendToSelectCamera(float BlendTime = 0.7f);

	/** Blend camera to the character create view (close-up on slot 0). */
	void BlendToCreateCamera(float BlendTime = 0.7f);

	/** Blend camera back to the login background view. */
	void BlendToLoginCamera(float BlendTime = 0.7f);

	/** Enable mouse-drag rotation for the create preview character. */
	void SetPreviewRotationEnabled(bool bEnabled) { bRotationEnabled = bEnabled; }

	/** Call from Tick to apply mouse drag rotation. */
	void TickPreviewRotation(float DeltaTime);


private:
	UPROPERTY()
	UMyGameInstance* GameInstanceRef = nullptr;

	UPROPERTY()
	TArray<ABasicPlayer*> SelectPreviewActors;

	UPROPERTY()
	ABasicPlayer* CreatePreviewActor = nullptr;

	int32 HighlightedIndex = -1;
	bool bRotationEnabled = false;
	float PreviewYawAccum = 0.0f;

	/** Original podium transforms — cached when SpawnCharacterPreviews runs so we can restore actors. */
	TArray<FTransform> CachedPodiumTransforms;

	/** Pending smooth-walk requests for preview characters. */
	struct FPreviewMoveRequest
	{
		ABasicPlayer* Actor           = nullptr;
		FVector       TargetLocation   = FVector::ZeroVector;
		/** Rotation the actor should hold once it arrives at TargetLocation. */
		FRotator      ArrivalRotation  = FRotator::ZeroRotator;
		bool          bActive          = false;
		/** Current animation speed, ramped smoothly up/down each tick. */
		float         CurrentSpeed     = 0.f;
		/** True once position is reached; we keep ticking to ramp anim to 0. */
		bool          bArrived         = false;
	};
	TArray<FPreviewMoveRequest> PendingMoves;

	/** Login-level nameplate canvas — created on demand, lives for the duration of the login level. */
	UPROPERTY()
	UNameplateCanvasWidget* LoginNameplateCanvas = nullptr;

	/** Spawn a headless BasicPlayer at a given transform (no input, no HUD). */
	ABasicPlayer* SpawnPreviewActor(const FTransform& Transform);

	/** Apply visual data from the DataTable to a preview actor. */
	void ApplyVisualToActor(ABasicPlayer* Actor, const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName);

	/** Apply server equipment preview to a preview actor via EquipmentVisualComponent. */
	void ApplyEquipmentToActor(ABasicPlayer* Actor, const TArray<FLoginEquipmentEntry>& Equipment);

	/**
	 * Convert a server equip_slot_id integer to the corresponding slug string.
	 * Returns empty string for unknown IDs.
	 */
	static FString SlotIdToSlug(int32 SlotId);

	/** Create the login-level nameplate canvas widget if not already created. */
	void EnsureLoginNameplateCanvas();

	/** Get the DataTable from GameInstance. */
	UDataTable* GetVisualTable() const;
};

