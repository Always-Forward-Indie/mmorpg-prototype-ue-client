#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/WIODataStructs.h"
#include "WorldInteractiveObjectActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

// Fired when the player enters/exits interaction range (for UI prompt)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWIOProximityChanged, AWorldInteractiveObjectActor*, Actor, bool, bInRange);

/**
 * AWorldInteractiveObjectActor
 *
 * Base actor class for World Interactive Objects.
 * Spawned by WorldObjectManager from server data.
 *
 * Automatic visual feedback (no BP code required):
 *  - Depleted  → mesh renders at DepletedOpacity (semi-transparent, default 0.35)
 *  - Disabled  → mesh hidden entirely (if bHideWhenDisabled)
 *  - Active    → mesh fully visible, original materials restored
 *  - Interaction success/fail → brief colour flash handled in C++ (Tick-driven)
 *
 * BP_On* events are optional hooks for extra VFX/SFX — not needed for base visuals.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API AWorldInteractiveObjectActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldInteractiveObjectActor();

	// ─── Initialization (called by WorldObjectManager) ──────────────────

	UFUNCTION(BlueprintCallable, Category = "WIO Actor")
	void InitializeFromServerData(const FWorldObjectData& InData);

	// ─── State management ────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "WIO Actor")
	void SetObjectState(EWIOState NewState, int32 InRespawnSec = 0);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	EWIOState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	bool IsInteractable() const { return CurrentState == EWIOState::Active; }

	// ─── Server data access ──────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	int32 GetObjectId() const { return ObjectData.ObjectId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	FString GetSlug() const { return ObjectData.Slug; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	FString GetNameKey() const { return ObjectData.NameKey; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	EWIOObjectType GetObjectType() const { return ObjectData.ObjectType; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	EWIOScope GetScope() const { return ObjectData.Scope; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	float GetInteractionRadius() const { return ObjectData.InteractionRadius; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	int32 GetChannelTimeSec() const { return ObjectData.ChannelTimeSec; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	const FWorldObjectData& GetObjectData() const { return ObjectData; }

	// ─── Respawn countdown ───────────────────────────────────────────────

	/** Returns remaining respawn seconds (0 if not depleted or no timer). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Actor")
	float GetRemainingRespawnTime() const;

	// ─── Interaction result callback ─────────────────────────────────────

	/** Called by WorldObjectManager when the server sends an interact result for this object. */
	void OnInteractResultReceived(const FWIOInteractResult& Result);

	// ─── Events (BP-assignable) ──────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "WIO Actor Events")
	FOnWIOProximityChanged OnProximityChanged;

	// ─── Visual configuration (editable per-BP or per-instance) ─────────

	/** Opacity multiplier applied to mesh materials when the object is Depleted.
	 *  0 = fully transparent, 1 = fully opaque. Default 0.35. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Actor|Visuals")
	float DepletedOpacity = 0.35f;

	/** If true, the mesh is hidden entirely when the object enters Disabled state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Actor|Visuals")
	bool bHideWhenDisabled = true;

	/** Tint colour used for the brief flash on successful interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Actor|Visuals")
	FLinearColor SuccessTintColour = FLinearColor(0.2f, 1.f, 0.2f, 1.f);

	/** Tint colour used for the brief flash on failed interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Actor|Visuals")
	FLinearColor FailTintColour = FLinearColor(1.f, 0.2f, 0.2f, 1.f);

	/** Duration (seconds) of the success/fail colour flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Actor|Visuals")
	float FlashDuration = 0.35f;

	// ─── Optional BP hooks (C++ visuals run first; override for extra VFX/SFX) ─

	/** Called AFTER C++ applies the state visual. Override for animations/SFX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO Actor Events")
	void BP_OnStateChanged(EWIOState NewState, EWIOState OldState);

	/** Called AFTER C++ applies the success flash. Override for reward VFX/SFX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO Actor Events")
	void BP_OnInteractionSuccess(const FWIOInteractResult& Result);

	/** Called AFTER C++ applies the fail flash. Override for error feedback. */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO Actor Events")
	void BP_OnInteractionFailed(const FString& ErrorCode);

	/** Called when respawn timer fires and state returns to Active. */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO Actor Events")
	void BP_OnRespawned();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ─── Components ──────────────────────────────────────────────────────

	/** Root scene component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WIO Components")
	USceneComponent* SceneRoot;

	/** Mesh — assign in your BP subclass or via DataTable mesh override. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WIO Components")
	UStaticMeshComponent* MeshComponent;

	/** Sphere trigger for proximity overlap detection. Radius set from server data. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WIO Components")
	USphereComponent* InteractionSphere;

private:
	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void StartRespawnTimer(int32 Seconds);
	void OnRespawnTimerComplete();

	/** Apply mesh opacity / visibility for a given state. */
	void ApplyStateVisuals(EWIOState State);

	/** Begin a colour flash on the mesh (alpha fades out over FlashDuration). */
	void StartFlash(FLinearColor Colour);

	UPROPERTY()
	FWorldObjectData ObjectData;

	EWIOState CurrentState = EWIOState::Active;

	// Respawn tracking
	double       RespawnEndTime  = 0.0;
	int32        RespawnTotalSec = 0;
	FTimerHandle RespawnTimerHandle;

	// Per-player done flag (for per_player scope objects)
	bool bLocalPlayerDone = false;

	// Flash state (driven by Tick)
	bool          bIsFlashing  = false;
	float         FlashElapsed = 0.f;
	FLinearColor  FlashColour;

	/** Dynamic material instances created from mesh materials at BeginPlay.
	 *  Used for per-state visual overrides (opacity, tint, emissive flash). */
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> CachedMIDs;
};
