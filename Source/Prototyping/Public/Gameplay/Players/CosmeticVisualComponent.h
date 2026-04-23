// Cosmetic Visual Component — manages cosmetic skeletal meshes (hair, facial hair, etc.)
// on a character using the Leader Pose pattern from AAA MMO games.
//
// How it works (AAA pattern — used in WoW, Aion, BDO):
//   Each cosmetic is a separate USkeletalMeshComponent that calls SetLeaderPoseComponent
//   on the character body mesh. This makes cosmetics copy bone transforms from the body
//   every frame — they animate in sync with no additional AnimInstance overhead.
//
// Visibility rules:
//   Each cosmetic row in CharacterCosmeticsDataTable declares which equipment slots
//   cause it to hide (e.g. "head" slot occupied → hair mesh hidden).
//   This component listens to equipment state changes and applies those rules automatically.
//
// Usage (local player):
//   1. Call SetDefaultCosmetics(VisualDef, CosmeticsTable) after body mesh is resolved.
//   2. Call BindEquipmentManager(EqMgr) once equipment is initialized.
//
// Usage (remote / preview actors):
//   1. Call SetDefaultCosmetics(VisualDef, CosmeticsTable).
//   2. Call ApplyEquipmentState(StateData) with the current equipment snapshot.
//      For live remote players, also call SetOwnerCharacterId + subscribe
//      HandleRemoteEquipmentState to EquipmentManager::OnRemoteEquipmentStateReceivedDelegate.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "CosmeticVisualComponent.generated.h"

class UDataTable;
class UEquipmentManager;
struct FCharacterVisualDefinition;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UCosmeticVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCosmeticVisualComponent();

	// ─── Initialization ────────────────────────────────────────────────────

	/**
	 * Spawn default cosmetic meshes from the visual definition (hair, facial hair, etc.).
	 * Safe to call before the body SkeletalMesh has finished async-loading —
	 * SetLeaderPoseComponent binds to the USkeletalMeshComponent pointer, not the asset.
	 *
	 * @param VisualDef       Row from CharacterVisualDefinitionsTable (class+race+gender).
	 * @param InCosmeticsTable CharacterCosmeticsDataTable from GameInstance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void SetDefaultCosmetics(const FCharacterVisualDefinition& VisualDef, UDataTable* InCosmeticsTable);

	/**
	 * Bind to a live EquipmentManager so visibility updates automatically when
	 * the player equips or unequips items. Use for the LOCAL player only.
	 * Immediately re-evaluates visibility from the manager's current state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void BindEquipmentManager(UEquipmentManager* InEquipmentManager);

	/**
	 * Apply a static equipment state snapshot to evaluate cosmetic visibility.
	 * Use for preview actors on the login screen and remote players.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void ApplyEquipmentState(const FEquipmentStateData& State);

	/**
	 * Filtered callback for PLAYER_EQUIPMENT_UPDATE packets (remote players).
	 * Checks OwnerCharacterId so only this component's owner triggers a refresh.
	 * Bind to EquipmentManager::OnRemoteEquipmentStateReceivedDelegate.
	 */
	UFUNCTION()
	void HandleRemoteEquipmentState(const FEquipmentStateData& State);

	/**
	 * Override a specific cosmetic slot with a new slug (for the character editor).
	 * Destroys the existing mesh for that slot and spawns the new one.
	 * Pass NAME_None as CosmeticSlug to remove the cosmetic without replacing it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void SetCosmeticSlot(FName CosmeticSlot, FName CosmeticSlug);

	// Set the character ID this component belongs to (needed for remote player filtering).
	void SetOwnerCharacterId(int32 InCharacterId) { OwnerCharacterId = InCharacterId; }
	int32 GetOwnerCharacterId() const { return OwnerCharacterId; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/**
	 * Spawn (or replace) the mesh component for the cosmetic identified by CosmeticSlug.
	 * Looks up the row in CosmeticsTable, creates USkeletalMeshComponent,
	 * calls SetLeaderPoseComponent on the body mesh, and async-loads the asset.
	 */
	void SpawnCosmeticMesh(FName CosmeticSlug);

	/** Destroy and remove the mesh component for a given cosmetic slot name. */
	void DestroySlotComponent(FName CosmeticSlot);

	/** Re-evaluate SetVisibility on all cosmetic meshes against OccupiedEquipSlots. */
	void EvaluateAllVisibility();

	/** Delegate callback for local player equipment changes. */
	UFUNCTION()
	void OnEquipmentStateChanged(const FEquipmentStateData& State);

	// ─── State ────────────────────────────────────────────────────────────

	/** cosmetic slot name ("hair") → active mesh component (UStaticMeshComponent or USkeletalMeshComponent) */
	UPROPERTY()
	TMap<FName, USceneComponent*> CosmeticMeshes;

	/** cosmetic slot name → currently active cosmetic slug (for hide-rule lookup) */
	TMap<FName, FName> SlotToSlug;

	/**
	 * cosmetic slug → equipment slots whose occupation hides this cosmetic.
	 * Cached from the DataTable row on SpawnCosmeticMesh so runtime evaluation is O(N).
	 */
	TMap<FName, TArray<FName>> SlugToHideRules;

	/** Equipment slot slugs currently occupied — updated on every state change. */
	TSet<FName> OccupiedEquipSlots;

	UPROPERTY()
	UDataTable* CosmeticsTable = nullptr;

	/** Non-null when bound to a local EquipmentManager (local player only). */
	UPROPERTY()
	UEquipmentManager* BoundEquipmentManager = nullptr;

	/** Character ID used to filter HandleRemoteEquipmentState (remote players only). */
	int32 OwnerCharacterId = 0;
};
