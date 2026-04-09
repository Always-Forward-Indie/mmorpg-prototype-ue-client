// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "Data/DataStructs.h"
#include <Gameplay/UI/MOBHeadInfo.h>
#include "Components/CapsuleComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Components/AudioComponent.h"
#include <Gameplay/UI/W_MOBHeadInfoWidget.h>
#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "Gameplay/Combat/ICombatable.h"
#include "BasicMOB.generated.h"

// Forward declarations
struct FSkillInitiationData;
struct FSkillResultData;
struct FEffectTickData;
struct FMobMoveEntryStruct;

struct FMobHealthUpdateStruct;

// Event declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMOBDataUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMOBDied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMOBTargetLost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMOBSkillInitiated, const FSkillInitiationData&, SkillData, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMOBSkillResult,     const FSkillResultData&,     SkillResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMOBEffectTick,      const FEffectTickData&,      EffectData);

/**
 *
 */
UCLASS()
class PROTOTYPING_API ABasicMOB : public ACharacter, public ICombatable
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB Data", meta = (AllowPrivateAccess = "true"))
	FMOBStruct MOBData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB Data", meta = (AllowPrivateAccess = "true"))
	float LastUpdateTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB Data", meta = (AllowPrivateAccess = "true"))
	FString lastTimestamp = "";

	// Combat system target tracking  
	int32 CurrentTargetId = 0;
	ECasterType CurrentTargetType = ECasterType::None;
	


	// Per-instance tracking for UpdateWidgetScale throttle
	float LastDisplayedWidgetScale = 0.0f;
	float LastDisplayedWidgetDistance = 0.0f;

public:
	// Sets default values for this character's properties
	ABasicMOB();

	// ICombatable interface implementation
	virtual int32 GetActorId_Implementation() const override 
	{ 
		if (MOBData.mobUniqueID.IsEmpty())
		{
			return 0;
		}
		int32 ConvertedId = FCString::Atoi(*MOBData.mobUniqueID);
		return ConvertedId > 0 ? ConvertedId : 0;
	}
	virtual ECasterType GetActorType_Implementation() const override { return ECasterType::Mob; }
	virtual FString GetActorTypeString_Implementation() const override { return TEXT("Mob"); }
	
	virtual int32 GetCurrentHealth_Implementation() const override { return MOBData.mobCurrentHealth; }
	virtual int32 GetMaxHealth_Implementation() const override;
	virtual int32 GetCurrentMana_Implementation() const override { return MOBData.mobCurrentMana; }
	virtual int32 GetMaxMana_Implementation() const override;
	
	virtual void SetCurrentHealth_Implementation(int32 NewHealth) override { SetMOBCurrentHealth(NewHealth); }
	virtual void SetCurrentMana_Implementation(int32 NewMana) override { SetMOBCurrentMana(NewMana); }
	
	virtual bool IsDead_Implementation() const override { return GetMOBIsDead(); }
	virtual void SetDead_Implementation(bool bNewDead) override;
	virtual void OnDeath_Implementation() override;
	
	virtual FVector GetCombatPosition_Implementation() const override { return GetActorLocation() + FVector(0, 0, 120); }
	
	virtual void SetTarget_Implementation(int32 TargetId, ECasterType TargetType) override;
	virtual void SetIsAggressiveState_Implementation(int32 TargetId, ECasterType TargetType, bool bIsAggressive) override;
	virtual void ClearTarget_Implementation() override;
	
	virtual void PlaySkillAnimation_Implementation(const FString& AnimationName, const FString& SkillSlug, float Duration = 0.0f) override;
	virtual void ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School, bool bIsMissed, bool bIsBlocked, const FString& SkillSlug) override;
	virtual void ShowHealingEffect_Implementation(int32 Healing, const FString& SkillSlug) override;
	virtual void ShowBuffEffect_Implementation(const FAppliedEffectData& Effect) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	class UMOBMovementComponent* MOBMovementComponent;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowDebugSpheres = false;

	void OnReceiveServerPacket(const FPositionDataStruct& MOBPosition);

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxAcceleration = 800.f; // ???????????? ????????? (??????/?^2)

	// ??????????? ???????? (??????/?), ????? ?? ??????????? ??????? ?????? ?? ????? ????????
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MinMoveSpeed = 580.f;

	// ????????? (??????), ??? ??????? ?? ??? snapping? ? ????? ??? ????????????
	UPROPERTY(EditAnywhere, Category = "Movement")
	float SnapDistance = 10.f;

	// Helper function to adjust mob position to ground
	// Helper function to adjust mob position to ground with priority control
	FVector AdjustToGround(const FVector& Location, float DeltaTime, float Priority = 1.0f);

	// Enables debug visualization for ground adjustment
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Debug")
	bool bDebugGroundAdjustment = false;

	// Event variable
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnMOBDataUpdated MOBDataUpdated;


	// Add this with your other timer handles
	FTimerHandle DamageFlagResetTimer;

	// Add this with your other configurable properties
	UPROPERTY(EditAnywhere, Category = "Damage")
	float DamageFlagResetTime = 0.5f; // Time in seconds before damage flag


	UPROPERTY()
	UW_MOBHeadInfoWidget* HeadWidget;

	// Constant movement speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float MoveSpeed = 200.0f; // Adjust as needed

	// Constant rotation speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float RotationSpeed = 1000.0f; // Adjust as needed

	// Interpolation speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float interpolationSpeedFactor = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Configuration", meta = (AllowPrivateAccess = "true"))
	float maxInterpolationSpeed = 1200.0f;

	UFUNCTION(BlueprintCallable, Category = "MOB")
	FMOBStruct GetMOBData() const;

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBData(const FMOBStruct& Data);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBId(const int& MOBId);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobTargetId(const int32& TargetId);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobTargetType(const FString& TargetType);


	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBUId(const FString& MOBUId);
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBZoneId(const int& MOBZoneId);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBName(const FString& MOBName);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBRace(const FString& MOBRace);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBLevel(const int& MOBLevel);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBCurrentHealth(const int& MOBCurrentHealth);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBCurrentMana(const int& MOBCurrentMana);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBPosition(const FPositionDataStruct& MOBPosition, const FString& PacketTimestamp);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBAttributes(const FAttributesDataStruct& MOBAttributes);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBAttribute(const FString& AttributeSlug, const FAttributeDataStruct& MOBAttribute);

	// set mob is dead
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBIsDead(const bool& MOBIsDead);

	// set mob is aggressive
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBIsAggressive(const bool& MOBIsAggressive);

	//set mob is moving
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBIsMoving(const bool& MOBIsMoving);
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobIsDamaged(const bool& bIsDamaged);

	// set last timestamp
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetLastTimestamp(const FString& Timestamp);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	//get mob name
	FString GetMobName() const;

	// get mob unique id
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FString GetMOBUId() const;

	// get mob id
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBId() const;

	// get mob race	
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FString GetMOBRace() const;

	// get mob level
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBLevel() const;

	// get mob current health
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBCurrentHealth() const;

	// get mob current mana
	UFUNCTION(BlueprintCallable, Category = "MOB")
	int GetMOBCurrentMana() const;

	// get mob position
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FVector GetMOBPosition() const;

	// get mob attributes
	UFUNCTION(BlueprintCallable, Category = "MOB")
	FAttributesDataStruct GetMOBAttributes() const;

	// get mob is dead
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsDead() const;

	// get mob is aggressive
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsAggressive() const;

	//get mob is moving
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsMoving() const;

	// get mob is damaged
	UFUNCTION(BlueprintCallable, Category = "MOB")
	bool GetMOBIsDamaged() const;


	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobNameText(UTextRenderComponent* MobNameTextComponent, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMobLevelText(UTextRenderComponent* MobLevelTextComponent, const FString& Level);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void SetMOBTag(const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "MOB")
	void MOBTextFaceToCamera(UTextRenderComponent* MobTextComponent);

	// Force update UI
	UFUNCTION(BlueprintCallable, Category = "MOB")
	void ForceUpdateUI();

	// Initialize UI with delay
	UFUNCTION()
	void InitializeUIDelayed();

	void Die();

	public:
		UPROPERTY(EditAnywhere, Category = "UI")
		UMOBHeadInfo* MobHeadInfo;

		// In your BasicMOB.h file add these with more appropriate values
		UPROPERTY(EditAnywhere, Category = "UI")
		float MinSize = 80.0f; // Smaller size when far away

		UPROPERTY(EditAnywhere, Category = "UI")
		float MaxSize = 250.0f; // Original size when close

		UPROPERTY(EditAnywhere, Category = "UI")
		float MinDistance = 700.0f; // Distance at which UI is full size

		UPROPERTY(EditAnywhere, Category = "UI")
		float MaxDistance = 2500.0f; // Distance at which UI is minimum size

		UPROPERTY(EditAnywhere, Category = "UI")
		float widgetScaleFactor = 1.0f; // Scale factor for the widget

		UPROPERTY(EditAnywhere, Category = "UI")
		float CurrentWidgetScale = 1.0f;

		UPROPERTY(EditAnywhere, Category = "UI")
		float InterpSpeedFactor = 5.0f;


		int LastHealth = 0;
		int LastMana = 0;
		bool bUIInitialized = false;
		bool LastAggressive = false;


	protected:
		// Called when the game starts or when spawned
		virtual void BeginPlay() override;

		// Called when the actor is being destroyed
		virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	public:	
		// Called every frame
		virtual void Tick(float DeltaTime) override;

		void UpdateWidgetScale(float DeltaTime);

		void UpdateWidgetPosition();

		// Called to bind functionality to input
		virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

		void SetupMobVisual(FName MobSlug);
		void SetupMobAudio(FName MobSlug);

		void PlayRandomIdleSound();

		void PlaySoundByName(FName SoundName);

		void PlayWalkRandomSound();

		void PlayRunRandomSound();

		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOBs Config")
		UDataTable* MobDefinitionTable;

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
		UAudioComponent* AudioComponentMain;

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
		UAudioComponent* AudioComponentSecond;

		// ?????
		UPROPERTY()
		TMap<FName, USoundBase*> SoundMap;

		UPROPERTY()
		TArray<USoundBase*> IdleSounds;

		UPROPERTY()
		TArray<USoundBase*> WalkSounds;

		UPROPERTY()
		TArray<USoundBase*> RunSounds;

		UPROPERTY()
		TArray<USoundBase*> AttackVoiceSounds;

		UPROPERTY()
		TArray<USoundBase*> CastVoiceSounds;

		UPROPERTY()
		TArray<USoundBase*> ReleaseVoiceSounds;

		FTimerHandle IdleSoundTimer;

	// Check if MOB can be harvested
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	bool CanBeHarvested() const;

	// Check if MOB has been harvested
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	bool HasBeenHarvested() const;

	// Set MOB as harvested
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void SetHarvested(bool bHarvested);

	// ?? New skill / combat system callbacks ???????????????????????????????
	void OnReceiveSkillInitiation(const FSkillInitiationData& SkillData);
	void OnReceiveSkillResult(const FSkillResultData& SkillResult);
	void OnReceiveEffectTick(const FEffectTickData& EffectData);
	void OnReceiveTargetLost();
	void OnReceiveMovePacket(const FMobMoveEntryStruct& MoveEntry, int64 ServerSendMs, int64 ClientRecvMs);

	void OnReceiveMobHealthUpdate(const FMobHealthUpdateStruct& HealthUpdate);

	// ?? Delegates broadcast to Blueprint / other systems ?????????????????
	UPROPERTY(BlueprintAssignable, Category = "MOB|Events")
	FOnMOBSkillInitiated OnSkillInitiated;

	UPROPERTY(BlueprintAssignable, Category = "MOB|Events")
	FOnMOBSkillResult OnSkillResult;

	UPROPERTY(BlueprintAssignable, Category = "MOB|Events")
	FOnMOBEffectTick OnEffectTick;

	UPROPERTY(BlueprintAssignable, Category = "MOB|Events")
	FOnMOBDied OnMOBDied;

	UPROPERTY(BlueprintAssignable, Category = "MOB|Events")
	FOnMOBTargetLost OnMOBTargetLost;

	// Delegate handle for anim-notify hit-point binding
	FDelegateHandle HitPointDelegateHandle;

	// Lock-out flag set after target is lost to prevent immediate re-aggro
	bool bAggroLockedOut = false;

	// Current skill name set during combat (used for VFX/SFX lookup)
	FString CurrentSkillName;

	// Cached icon loaded from MobDefinitionTable
	UPROPERTY()
	UTexture2D* CachedIcon = nullptr;

	// Cached combat hit height from MobDefinitionTable
	float CachedCombatHitHeight = 120.0f;
};
