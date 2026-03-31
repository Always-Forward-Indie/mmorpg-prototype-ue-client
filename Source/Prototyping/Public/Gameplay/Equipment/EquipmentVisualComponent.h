#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "EquipmentVisualComponent.generated.h"

class UItemManager;
class UEquipmentManager;
class UNiagaraComponent;

/**
 * EquipmentVisualComponent
 *
 * Attached to ABasicPlayer.
 * Listens to UEquipmentManager::OnEquipmentStateChangedDelegate and
 * spawns / destroys UStaticMeshComponent (or USkeletalMeshComponent) children
 * attached to the character skeleton sockets so that equipped items are
 * visually represented on the character mesh.
 *
 * Also spawns persistent Niagara VFX (EquippedIdleVFX) on items that have
 * a glow / enchantment aura defined in the ItemVisualsDataTable.
 *
 * Data pipeline:
 *   EquipmentManager::OnEquipmentStateChangedDelegate
 *       -> RefreshAllSlots()
 *           -> for each slot: lookup FItemVisualData by itemSlug via UItemManager
 *               -> AttachEquippedMesh() / ClearSlotMesh()
 *               -> SpawnEquippedIdleVFX() / DestroySlotVFX()
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UEquipmentVisualComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentVisualComponent();

    // Called once after the owning player has set up its managers.
    // Binds to EquipmentManager delegate and immediately refreshes visuals
    // from the current equipment state.
    UFUNCTION(BlueprintCallable, Category = "Equipment Visuals")
    void Initialize(UEquipmentManager* InEquipmentManager, UItemManager* InItemManager);

    // Lightweight init for remote players: only stores ItemManager reference.
    // No delegate binding — visuals are pushed directly via RefreshAllSlots().
    UFUNCTION(BlueprintCallable, Category = "Equipment Visuals")
    void InitializeForRemotePlayer(UItemManager* InItemManager);

    // Force-rebuild all slot meshes from the current EquipmentManager state.
    // Called automatically when the equipment state changes.
    UFUNCTION()
    void RefreshAllSlots(const FEquipmentStateData& State);

    // Callback for PLAYER_EQUIPMENT_UPDATE packets (remote players).
    // Filters by OwnerCharacterId so only this component's owner is updated.
    UFUNCTION()
    void HandleRemoteEquipmentState(const FEquipmentStateData& State);

    // Set the character ID this component belongs to (needed for remote filtering).
    void SetOwnerCharacterId(int32 InCharacterId) { OwnerCharacterId = InCharacterId; }
    int32 GetOwnerCharacterId() const { return OwnerCharacterId; }

    // Remove the mesh for a single slot (e.g. on unequip before the full state update arrives).
    UFUNCTION(BlueprintCallable, Category = "Equipment Visuals")
    void ClearSlotMesh(const FString& SlotSlug);

    // Remove all equipped meshes (e.g. on death / level transition).
    UFUNCTION(BlueprintCallable, Category = "Equipment Visuals")
    void ClearAllMeshes();

    // Get the mesh component for a given equipment slot (e.g. for weapon trail sockets).
    UFUNCTION(BlueprintCallable, Category = "Equipment Visuals")
    USceneComponent* GetSlotComponent(const FString& SlotSlug) const;

    // Get the item slug currently equipped in a slot (e.g. "iron_sword" in "main_hand").
    UFUNCTION(BlueprintCallable, Category = "Equipment Visuals")
    FString GetItemSlugForSlot(const FString& SlotSlug) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // Attach or swap the mesh for one slot based on FItemVisualData.
    // Returns the newly created component, or null if the slot has no visual.
    USceneComponent* AttachEquippedMesh(const FString& SlotSlug,
                                        const FString& ItemSlug);

    // Spawn persistent idle VFX (EquippedIdleVFX) on the socket for this slot.
    void SpawnEquippedIdleVFX(const FString& SlotSlug, const struct FItemVisualData& VisualData,
                              USkeletalMeshComponent* CharMesh);

    // Destroy and remove the component stored for SlotSlug (if any).
    void DestroySlotComponent(const FString& SlotSlug);

    // Destroy and remove the VFX component stored for SlotSlug (if any).
    void DestroySlotVFX(const FString& SlotSlug);

    // One dynamic mesh component per equipment slot slug.
    // Key = equip slot slug (e.g. "main_hand", "chest").
    // Value = the UStaticMeshComponent / USkeletalMeshComponent we spawned.
    UPROPERTY()
    TMap<FString, USceneComponent*> SlotComponents;

    // One Niagara VFX component per equipment slot slug (EquippedIdleVFX).
    UPROPERTY()
    TMap<FString, UNiagaraComponent*> SlotVFXComponents;

    // Tracks which item slug is in each slot (for weapon trail / VFX lookups).
    UPROPERTY()
    TMap<FString, FString> SlotItemSlugs;

    UPROPERTY()
    UEquipmentManager* EquipmentManager = nullptr;

    UPROPERTY()
    UItemManager* ItemManager = nullptr;

    // Character ID this component belongs to.
    // Used to filter PLAYER_EQUIPMENT_UPDATE packets for remote players.
    int32 OwnerCharacterId = 0;
};
