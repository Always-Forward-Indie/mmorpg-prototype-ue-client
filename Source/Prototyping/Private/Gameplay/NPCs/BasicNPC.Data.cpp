#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/NPCs/NPCAnimInstance.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/UI/NPCNameplateComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Data/EntityAudioRepository.h"
#include "GameFramework/Pawn.h"

void ABasicNPC::SetNPCData(const FNPCStruct& Data)
{
	if (NPCData.id == 0 && Data.id > 0)
	{
		NPCData = Data;
		NPCDataUpdated.Broadcast();
		
		UE_LOG(LogTemp, Warning, TEXT("NPC Data set for %s (ID:%d): HP=%d, MP=%d"), 
			*NPCData.name, NPCData.id, NPCData.stats.health.current, NPCData.stats.mana.current);

		// Set actor location from NPC position data
		FVector NewLocation(NPCData.position.positionX, NPCData.position.positionY, NPCData.position.positionZ);
		SetActorLocation(NewLocation);
		SnapToGround();
		
		// Set actor rotation
		FRotator NewRotation(0.0f, NPCData.position.rotationZ, 0.0f);
		SetActorRotation(NewRotation);

		// Mark UI as ready for initialization when UI components are added
		bUIInitialized = false;
		LastHealth = NPCData.stats.health.current;
		LastMana = NPCData.stats.mana.current;

		// Initialize nameplate component with NPC data
		if (NPCNameplateComponent)
		{
			NPCNameplateComponent->InitialiseFromNPCData(NPCData, false);
		}

		// Wire up ambient speech pools if available
		if (UNPCAmbientSpeechComponent* ASComp = FindComponentByClass<UNPCAmbientSpeechComponent>())
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(GI))
				{
					FAmbientSpeechNPCData AmbientData;
					if (MyGI->AmbientSpeechManager &&
						MyGI->AmbientSpeechManager->GetNPCAmbientData(NPCData.id, AmbientData))
					{
						ASComp->SetAmbientData(AmbientData);
					}
				}
			}
		}

		// Setup visual and audio based on NPC slug
		if (!NPCData.slug.IsEmpty())
		{
			FName NPCSlugName(*NPCData.slug);
			SetupNPCVisual(NPCSlugName);
			SetupNPCAudio(NPCSlugName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("NPC slug is empty for NPC %s (ID:%d), visual/audio setup skipped"), *NPCData.name, NPCData.id);
		}
	}
	else
	{
		if (NPCData.id != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("NPC Data already set for %s (ID:%d)"), *NPCData.name, NPCData.id);
		}
		else if (Data.id <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid NPC ID (%d) provided"), Data.id);
		}
	}
}

void ABasicNPC::SetNPCId(int32 NPCId)
{
	NPCData.id = NPCId;
}

void ABasicNPC::UpdateNPCQuestData(const TArray<FNPCQuestEntry>& NewQuests)
{
	NPCData.quests = NewQuests;

	// Refresh nameplate interaction state
	if (UNPCNameplateComponent* NP = FindComponentByClass<UNPCNameplateComponent>())
	{
		NP->InitialiseFromNPCData(NPCData, true);
	}

	UE_LOG(LogTemp, Log, TEXT("NPC %s (ID:%d): Quest data updated, %d quests"),
		*NPCData.name, NPCData.id, NewQuests.Num());
}

void ABasicNPC::SetNPCName(const FString& NPCName)
{
	NPCData.name = NPCName;
}

void ABasicNPC::SetNPCSlug(const FString& NPCSlug)
{
	NPCData.slug = NPCSlug;
}

void ABasicNPC::SetNPCRace(const FString& NPCRace)
{
	NPCData.race = NPCRace;
}

void ABasicNPC::SetNPCLevel(int32 NPCLevel)
{
	NPCData.level = NPCLevel;
}

void ABasicNPC::SetNPCType(const FString& NPCType)
{
	NPCData.npcType = NPCType;
}

void ABasicNPC::SetNPCInteractable(bool bInteractable)
{
	NPCData.isInteractable = bInteractable;
}

void ABasicNPC::SetNPCDialogueId(const FString& DialogueId)
{
	NPCData.dialogueId = DialogueId;
}

void ABasicNPC::SetNPCQuestId(const FString& QuestId)
{
	NPCData.questId = QuestId;
}

void ABasicNPC::SetNPCPosition(const FPositionDataStruct& Position)
{
	NPCData.position = Position;
	
	// Update actor location
	FVector NewLocation(Position.positionX, Position.positionY, Position.positionZ);
	SetActorLocation(NewLocation);
	SnapToGround();
	
	// Update actor rotation
	FRotator NewRotation(0.0f, Position.rotationZ, 0.0f);
	SetActorRotation(NewRotation);
}

void ABasicNPC::SetNPCAttributes(const TArray<FAttributeDataStruct>& Attributes)
{
	NPCData.attributes = Attributes;
}

void ABasicNPC::SetNPCStats(const FNPCHealthManaStruct& Stats)
{
	NPCData.stats = Stats;
	
	// Update tracking variables for UI when it becomes available
	LastHealth = Stats.health.current;
	LastMana = Stats.mana.current;
	
	UE_LOG(LogTemp, Log, TEXT("NPC %s (ID:%d) stats updated: HP=%d/%d, MP=%d/%d"), 
		*NPCData.name, NPCData.id, Stats.health.current, Stats.health.max, Stats.mana.current, Stats.mana.max);
}

FVector ABasicNPC::GetNPCPosition() const
{
	return FVector(NPCData.position.positionX, NPCData.position.positionY, NPCData.position.positionZ);
}

