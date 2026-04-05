#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/DataStructs.h"
#include "Engine/DataTable.h"
#include "Engine/AssetManager.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "BasicNPC.generated.h"

// Forward declarations
class UNPCHeadInfo;
class UW_NPCHeadInfoWidget;
class USoundBase;
class UNPCNameplateComponent;

// Delegate for NPC data updates
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCDataUpdated);

/**
 * Basic NPC class that represents non-player characters in the game world
 * Follows SOLID principles and integrates with the existing architecture
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API ABasicNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ABasicNPC();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// NPC Data Management
	UFUNCTION(BlueprintCallable, Category = "NPC")
	FNPCStruct GetNPCData() const { return NPCData; }

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCData(const FNPCStruct& Data);

	/** Update only the quest entries in NPC data and refresh the nameplate icon. */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void UpdateNPCQuestData(const TArray<FNPCQuestEntry>& NewQuests);

	// Individual setters for NPC properties
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCId(int32 NPCId);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCName(const FString& NPCName);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCSlug(const FString& NPCSlug);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCRace(const FString& NPCRace);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCLevel(int32 NPCLevel);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCType(const FString& NPCType);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCInteractable(bool bInteractable);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCDialogueId(const FString& DialogueId);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCQuestId(const FString& QuestId);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCPosition(const FPositionDataStruct& Position);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCAttributes(const TArray<FAttributeDataStruct>& Attributes);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCStats(const FNPCHealthManaStruct& Stats);

	// Getters
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	int32 GetNPCId() const { return NPCData.id; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FString GetNPCName() const { return NPCData.name; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FString GetNPCSlug() const { return NPCData.slug; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FString GetNPCRace() const { return NPCData.race; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	int32 GetNPCLevel() const { return NPCData.level; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FString GetNPCType() const { return NPCData.npcType; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	bool IsNPCInteractable() const { return NPCData.isInteractable; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FString GetNPCDialogueId() const { return NPCData.dialogueId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FString GetNPCQuestId() const { return NPCData.questId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FVector GetNPCPosition() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	TArray<FAttributeDataStruct> GetNPCAttributes() const { return NPCData.attributes; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FNPCHealthManaStruct GetNPCStats() const { return NPCData.stats; }

	// Interaction methods
	UFUNCTION(BlueprintCallable, Category = "NPC")
	virtual void OnPlayerInteract(class APlayerController* InteractingPlayer);

	UFUNCTION(BlueprintImplementableEvent, Category = "NPC")
	void OnInteractionReceived(APlayerController* InteractingPlayer);

	// Audio methods
	UFUNCTION(BlueprintCallable, Category = "NPC Audio")
	void PlaySoundByName(FName SoundName);

	UFUNCTION(BlueprintCallable, Category = "NPC Audio")
	void PlayRandomIdleSound();

	UFUNCTION(BlueprintCallable, Category = "NPC Audio")
	void PlayGreetingSound();

	UFUNCTION(BlueprintCallable, Category = "NPC Audio")
	void PlayFarewellSound();

	// Visual setup methods
	UFUNCTION(BlueprintCallable, Category = "NPC Visual")
	void SetupNPCVisual(FName NPCSlug);

	UFUNCTION(BlueprintCallable, Category = "NPC Audio")
	void SetupNPCAudio(FName NPCSlug);

	// UI methods
	void UpdateWidgetScale(float DeltaTime);
	void UpdateWidgetPosition();
	void ForceUpdateUI();

	// Events
	UPROPERTY(BlueprintAssignable, Category = "NPC Events")
	FOnNPCDataUpdated NPCDataUpdated;

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC UI")
	float MinDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC UI")
	float MaxDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC UI")
	float widgetScaleFactor = 1.0f;

public:
	// Data tables for NPC definitions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
	class UDataTable* NPCDefinitionTable;

protected:
	// Core NPC data
	UPROPERTY(BlueprintReadOnly, Category = "NPC Data")
	FNPCStruct NPCData;

	// Nameplate component - registers with central NameplateManager for screen-space rendering
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC UI")
	UNPCNameplateComponent* NPCNameplateComponent;

	// Audio Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Audio")
	class UAudioComponent* AudioComponentMain;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Audio")
	class UAudioComponent* AudioComponentSecond;

	// ---- Audio assets ----
	UPROPERTY() TMap<FName, USoundBase*> SoundMap;
	UPROPERTY() TArray<USoundBase*> IdleSounds;
	UPROPERTY() TArray<USoundBase*> WalkSounds;   // NEW
	UPROPERTY() TArray<USoundBase*> RunSounds;    // NEW

	// ---- UI state tracking ----
	bool  bUIInitialized = false;
	int32 LastHealth = -1;
	int32 LastMana = -1;
	float CurrentWidgetScale = 1.0f;
	float LastUpdateTime = 0.0f;

private:
	// Initialize UI with delay
	void InitializeUIDelayed();

	// Idle sound scheduling
	void ScheduleNextIdleSound();
	FTimerHandle IdleSoundTimerHandle;
};