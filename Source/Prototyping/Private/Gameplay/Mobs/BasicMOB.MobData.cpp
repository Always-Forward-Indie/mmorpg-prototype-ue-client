// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "UI/UIManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Data/EffectDefinitionTable.h"
#include "Utils/AudioSpawnHelpers.h"

void ABasicMOB::SetMOBTag(const FString& Tag)
{
	Tags.Add(FName(*Tag));
}

FMOBStruct ABasicMOB::GetMOBData() const
{
	return MOBData;
}

void ABasicMOB::SetMOBData(const FMOBStruct& Data)
{
	if (MOBData.mobID == 0 && Data.mobID > 0 && !Data.mobUniqueID.IsEmpty())
	{
		MOBData = Data;
		// Cache the "born-aggressive" flag from the server definition.
		// This never changes: naturally-aggressive mobs stay red after losing a target.
		bIsNaturallyAggressive = Data.bIsAggressive;
		MOBDataUpdated.Broadcast();
		
		UE_LOG(LogTemp, Warning, TEXT("MOB Data set for %s (ID:%d, UID:%s): HP=%d, MP=%d"), 
			*MOBData.mobName, MOBData.mobID, *MOBData.mobUniqueID, MOBData.mobCurrentHealth, MOBData.mobCurrentMana);

		// Register with MOB registry so mobMoveUpdate packets can find this actor by UID.
		// BeginPlay fires before SetMOBData so the uid is 0 there; we do it here instead.
		int32 ActorId = GetActorId_Implementation();
		if (ActorId > 0)
		{
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
			{
				if (GameInstance->MOBManager)
				{
					GameInstance->MOBManager->RegisterMob(ActorId, this);
				}
			}
		}

		// Register with combat system now that we have valid data
		// Only register if we have a valid actor ID (converted from UID)
		if (ActorId > 0)
		{
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
			{
				if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
				{
					// �������� ��� ������ ������� ����� ������������
					if (IsValid(this) && !IsActorBeingDestroyed())
					{
						// Create TScriptInterface for registration
						TScriptInterface<ICombatable> CombatableInterface;
						CombatableInterface.SetObject(this);
						CombatableInterface.SetInterface(this);
						
						CombatManager->RegisterCombatable(CombatableInterface);
						UE_LOG(LogTemp, Warning, TEXT("MOB %d (UID:%s) registered with combat system"), ActorId, *MOBData.mobUniqueID);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("MOB %d (UID:%s) is invalid or being destroyed, not registering"), ActorId, *MOBData.mobUniqueID);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MOB has invalid ActorId (%d) from UID '%s', not registering with combat system"), 
				ActorId, *MOBData.mobUniqueID);
		}

		// ������������� ��������� UI ��� ������ ��������� ������
		ForceUpdateUI();
	}
	else
	{
		if (MOBData.mobID != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("MOB Data already set for %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
		}
		else if (Data.mobID <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid MOB ID (%d) provided"), Data.mobID);
		}
		else if (Data.mobUniqueID.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Empty MOB UID provided for MOB ID %d"), Data.mobID);
		}
	}

	if (MOBData.bIsDead)
	{
		Die();
	}
}

void ABasicMOB::SetMOBId(const int& MOBId)
{
	MOBData.mobID = MOBId;
}

void ABasicMOB::SetMOBUId(const FString& MOBUId)
{
	MOBData.mobUniqueID = MOBUId;
}

void ABasicMOB::SetMobTargetId(const int32& TargetId)
{
	MOBData.mobTargetId = TargetId;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->SetTargetId(TargetId);
	}
}

void ABasicMOB::SetMobTargetType(const FString& TargetType)
{
	MOBData.mobTargetType = TargetType;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->SetTargetType(TargetType);
	}
}

void ABasicMOB::SetMOBZoneId(const int& MOBZoneId)
{
	MOBData.mobZoneID = MOBZoneId;
}

void ABasicMOB::SetMOBName(const FString& MOBName)
{
	MOBData.mobName = MOBName;
}

void ABasicMOB::SetMOBRace(const FString& MOBRace)
{
	MOBData.mobRace = MOBRace;
}

void ABasicMOB::SetMOBLevel(const int& MOBLevel)
{
	MOBData.mobLevel = MOBLevel;
}

void ABasicMOB::SetMOBCurrentHealth(const int& MOBCurrentHealth)
{
	MOBData.mobCurrentHealth = MOBCurrentHealth;
	ForceUpdateUI();
}

void ABasicMOB::SetMOBCurrentMana(const int& MOBCurrentMana)
{
	MOBData.mobCurrentMana = MOBCurrentMana;
	ForceUpdateUI();
}

// Improve the position setting function
void ABasicMOB::SetMOBPosition(const FPositionDataStruct& MOBPosition, const FString& PacketTimestamp)
{
	// Store the original position for reference
	MOBData.mobPosition = MOBPosition;
}

void ABasicMOB::SetMOBAttributes(const FAttributesDataStruct& MOBAttributes)
{
	MOBData.mobAttributes = MOBAttributes;
	
	// ���� UI ��� �� ��������������� � � ��� ���� ������, ������������� ��������� UI
	if (!bUIInitialized && MOBData.mobID != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMOBAttributes: Force updating UI for MOB %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
		ForceUpdateUI();
	}
}

void ABasicMOB::SetMOBAttribute(const FString& AttributeSlug, const FAttributeDataStruct& MOBAttribute)
{
	MOBData.mobAttributes.attributesData.Add(AttributeSlug, MOBAttribute);
	
	// ���� ��������� �������� �������� (��������/����) � UI �� ���������������, ��������� UI
	if (!bUIInitialized && MOBData.mobID != 0 && (AttributeSlug == TEXT("max_health") || AttributeSlug == TEXT("max_mana")))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMOBAttribute: Force updating UI for MOB %s (ID:%d) after setting %s"), *MOBData.mobName, MOBData.mobID, *AttributeSlug);
		ForceUpdateUI();
	}
}

void ABasicMOB::SetMOBIsDead(const bool& bIsDead)
{
	MOBData.bIsDead = bIsDead;
	RefreshHarvestableVFX();
}

void ABasicMOB::SetMOBIsAggressive(const bool& bIsAggressive)
{
	MOBData.bIsAggressive = bIsAggressive;
}

void ABasicMOB::SetMOBIsMoving(const bool& bIsMoving)
{
	MOBData.bIsMoving = bIsMoving;
}

void ABasicMOB::SetMobIsDamaged(const bool& bIsDamaged)
{
	if (!MOBData.bIsDead)
	{
		MOBData.bIsGotDamage = bIsDamaged;

		// If damage flag is set to true, schedule it to be reset after a delay
		if (bIsDamaged && GetWorld())
		{
			// Cancel any existing timers for damage reset
			GetWorld()->GetTimerManager().ClearTimer(DamageFlagResetTimer);

			// Set a timer to reset the damage flag after the specified time
			GetWorld()->GetTimerManager().SetTimer(
				DamageFlagResetTimer,
				[this]() {
					MOBData.bIsGotDamage = false;
					UE_LOG(LogTemp, Verbose, TEXT("Damage flag auto-reset for MOB %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
				},
				DamageFlagResetTime,
				false); // false = no looping
		}
	}
	else
	{
		MOBData.bIsGotDamage = false; // ���� ��� �����, �� �� ����� ���� ���������
	}
}

// Set last timestamp for the MOB
void ABasicMOB::SetLastTimestamp(const FString& Timestamp)
{
	lastTimestamp = Timestamp;
}

FString ABasicMOB::GetMobName() const
{
	return MOBData.mobName;
}

FString ABasicMOB::GetMOBUId() const
{
	return MOBData.mobUniqueID;
}

int ABasicMOB::GetMOBId() const
{
	return MOBData.mobID;
}

FString ABasicMOB::GetMOBRace() const
{
	return MOBData.mobRace;
}

int ABasicMOB::GetMOBLevel() const
{
	return MOBData.mobLevel;
}

int ABasicMOB::GetMOBCurrentHealth() const
{
	return MOBData.mobCurrentHealth;
}

int ABasicMOB::GetMOBCurrentMana() const
{
	return MOBData.mobCurrentMana;
}

FVector ABasicMOB::GetMOBPosition() const
{
	return FVector(MOBData.mobPosition.positionX, MOBData.mobPosition.positionY, MOBData.mobPosition.positionZ);
}

FAttributesDataStruct ABasicMOB::GetMOBAttributes() const
{
	return MOBData.mobAttributes;
}

bool ABasicMOB::GetMOBIsDead() const
{
	return MOBData.bIsDead;
}

bool ABasicMOB::GetMOBIsAggressive() const
{
	return MOBData.bIsAggressive;
}

//get mob is moving
bool ABasicMOB::GetMOBIsMoving() const
{
	return MOBData.bIsMoving;
}

//get mob is damaged
bool ABasicMOB::GetMOBIsDamaged() const
{
	if (!MOBData.bIsDead)
	{
		return MOBData.bIsGotDamage;
	}

	return false;
}

