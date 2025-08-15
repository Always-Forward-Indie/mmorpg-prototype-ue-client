// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ItemStruct.h"
#include "DroppedItemActor.generated.h"

UCLASS()
class PROTOTYPING_API ADroppedItemActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADroppedItemActor();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Set the item data
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	void SetItemData(const FDroppedItemStruct& InItemData);

	// Get the item data
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	FDroppedItemStruct GetItemData() const { return ItemData; }

	// Get the item base data
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	FItemBaseStruct GetItemBaseData() const { return ItemData.item; }

	// Get the item UID
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	int32 GetItemUID() const { return ItemData.uid; }

	// Attempt to pick up this item
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	bool AttemptPickup();

	// Handle interaction with this item
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	void Interact();

	// Get the pickup radius
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	float GetPickupRadius() const { return PickupRadius; }

	// Check if the item can be picked up
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	bool CanBePickedUp() const { return ItemData.canBePickedUp; }

	// Get the item name for UI display
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	FString GetItemName() const { return ItemData.item.name; }

	// Get the item rarity (from attributes)
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	int32 GetItemRarity() const;

	// C++ implementation for SetupItemVisuals when no BP implementation exists
	virtual void SetupItemVisuals_Implementation();

	// Called when the item successfully updates its visuals
	UFUNCTION(BlueprintCallable, Category = "Dropped Item")
	void OnVisualsSetup();

	// Try to start a trajectory animation from the mob that dropped this item
	void TryStartTrajectoryFromMob();

	// Set up trajectory animation from a source location to the target position
	void SetupTrajectoryAnimation(const FVector& SourceLocation);

	// Find the ground level at a specific world location
	float FindGroundLevelAt(const FVector& Location);

	FItemVisualData VisualData;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// The root component for the dropped item
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	// The static mesh component for the dropped item
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* ItemMesh;

	// The collision sphere for the dropped item
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* InteractionSphere;

	// The particle system component for the dropped item
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UParticleSystemComponent* ItemParticles;

	// The radius within which the item can be picked up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	float PickupRadius = 200.0f;

	// Drop animation parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float DropHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float DropAnimationDuration = 0.5f;

private:
	// The item data for this dropped item
	UPROPERTY()
	FDroppedItemStruct ItemData;

	// Initial position for animation
	FVector InitialPosition;

	// Target position on the ground
	FVector TargetPosition;

	// Random horizontal offset for drop direction
	FVector DropHorizontalOffset;

	// Time when the drop animation started
	float DropStartTime;

	// Whether the drop animation is currently active
	bool bIsDropAnimationActive;

	// Is the visual setup done?
	UPROPERTY()
	bool bVisualsSetupComplete;

	bool bIsSettling = false;
	float SettlingStartTime = 0.0f;
	FVector LastAnimatedPosition;
	FRotator LastAnimatedRotation;
	FRotator TargetRotation;
};