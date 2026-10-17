// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "UI/UIManager.h"
#include "UI/NotificationToastWidget.h"
#include "UI/ChatWidget.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/ItemStruct.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Utils/PlayerAttributeParser.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/UI/CastBarWidget.h"
#include "Gameplay/Combat/BaseMMOProjectile.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Data/EffectDefinitionTable.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "Data/CharacterVisualData.h"
#include "Engine/AssetManager.h"

// в”Ђв”Ђв”Ђ IWorldInteractable (remote player targeting) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ

EInteractableType ABasicPlayer::GetInteractableType() const
{
    // Only other clients appear as interactable remote players.
    return playerData.isOtherClient ? EInteractableType::RemotePlayer : EInteractableType::None;
}

FText ABasicPlayer::GetInteractableDisplayName() const
{
    const FString Name  = playerData.characterData.characterName;
    const int32   Level = playerData.characterData.characterLevel;
    if (Level > 0)
        return FText::FromString(FString::Printf(TEXT("%s  [%d]"), *Name, Level));
    return FText::FromString(Name);
}

// в”Ђв”Ђв”Ђ Cursor Interaction в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ

bool ABasicPlayer::IsUIBlockingInteraction() const
{
	return UIManager && IsValid(UIManager) && UIManager->HasCursorOverWindowContent();
}

float ABasicPlayer::GetInteractionRange() const
{
    return CursorInteractionComponent ? CursorInteractionComponent->GetInteractionRange() : 280.f;
}

void ABasicPlayer::DispatchCursorSelect(AActor* Target, EInteractableType Type)
{
    if (!Target)
    {
        return;
    }

    if (playerData.characterData.bIsDead)
    {
        return;
    }

    switch (Type)
    {
    case EInteractableType::MOB_Alive:
    case EInteractableType::MOB_Harvestable:
    case EInteractableType::MOB_Harvested:
        if (ABasicMOB* Mob = Cast<ABasicMOB>(Target))
        {
            // Select only вЂ” no auto-attack on single click
            SetLockedTarget(Mob);
        }
        break;

    case EInteractableType::NPC:
    case EInteractableType::DroppedItem:
        // Visual lock so the floor circle shows, no combat action
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(Target, Type);
        break;

    case EInteractableType::RemotePlayer:
        // Target the remote player for heals/buffs
        if (ABasicPlayer* OtherPlayer = Cast<ABasicPlayer>(Target))
        {
            SetLockedPlayerTarget(OtherPlayer);
        }
        break;

    default:
        break;
    }
}

void ABasicPlayer::DispatchCursorInteract(AActor* Target, EInteractableType Type)
{
    if (!Target) return;

    if (playerData.characterData.bIsDead)
    {
        return;
    }

    const float Range = GetInteractionRange();

    // Measure from the nearest point on the target's capsule (or actor origin as fallback)
    // so the player stops at the surface of the actor, not at its pivot (which can be
    // at chest/mid height, causing the player to stop 60-80 cm too far away).
    float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
    {
        if (const UCapsuleComponent* Cap = TargetChar->GetCapsuleComponent())
        {
            Dist = FMath::Max(0.f, Dist - Cap->GetScaledCapsuleRadius());
        }
    }
    const bool bInRange = (Dist <= Range);

    switch (Type)
    {
    // в”Ђв”Ђ MOB: alive в†’ attack в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::MOB_Alive:
        if (ABasicMOB* Mob = Cast<ABasicMOB>(Target))
        {
            SetLockedTarget(Mob);
            if (bInRange)
            {
                bIsAutoAttacking = true;
                DoAutoAttack();
            }
            else
            {
                PendingInteraction       = EPendingInteraction::AutoAttack;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ MOB: harvestable / harvested в†’ harvest or inspect в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::MOB_Harvestable:
    case EInteractableType::MOB_Harvested:
        if (ABasicMOB* Mob = Cast<ABasicMOB>(Target))
        {
            SetLockedTarget(Mob);

            float HarvestRange = Range;
            UHarvestManager* HMgr = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;
            if (HMgr)
            {
                HarvestRange = HMgr->GetMaxHarvestDistance();
            }

            if (HMgr && (Dist <= HarvestRange))
            {
                HMgr->TryHarvestSpecificCorpse(Mob);
            }
            else
            {
                PendingInteraction       = EPendingInteraction::Harvest;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ NPC в†’ open dialogue в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::NPC:
        if (ABasicNPC* NPC = Cast<ABasicNPC>(Target))
        {
            if (CursorInteractionComponent)
                CursorInteractionComponent->SetVisualLock(Target, Type);

            UDialogueManager* DlgMgr = MyGameInstance ? MyGameInstance->GetDialogueManager() : nullptr;
            if (bInRange)
            {
                if (DlgMgr)
                {
                    DlgMgr->OpenDialogue(NPC->GetNPCId());
                }
            }
            else
            {
                PendingInteraction       = EPendingInteraction::TalkNPC;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ DroppedItem в†’ pick up в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::DroppedItem:
        if (ADroppedItemActor* Item = Cast<ADroppedItemActor>(Target))
        {
            if (CursorInteractionComponent)
                CursorInteractionComponent->SetVisualLock(Target, Type);

            const float PickupRange = CursorInteractionComponent ? CursorInteractionComponent->GetItemPickupRange() : 180.f;
            if (Dist <= PickupRange)
            {
                if (InventoryManager)
                    InventoryManager->PickupSpecificItem(Item);
            }
            else
            {
                PendingInteraction       = EPendingInteraction::PickupItem;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ RemotePlayer в†’ inspect (placeholder) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::RemotePlayer:
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(Target, Type);
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Double-click on remote player вЂ” inspect not yet implemented"));
        break;

    default:
        break;
    }
}

void ABasicPlayer::HandleInteractiveHoverChanged(AActor* OldTarget, AActor* NewTarget, EInteractableType NewType)
{
	if (UIManager)
	{
		UIManager->ShowInteractionHint(NewType);
	}
}

void ABasicPlayer::UpdateProximityHint()
{
	if (!UIManager || !MyGameInstance) return;
	if (!GetWorld()) return;

	if (CursorInteractionComponent && IsValid(CursorInteractionComponent->GetHoveredActor()))
	{
		return;
	}

	AActor*            Nearest = nullptr;
	EInteractableType  NearestType = EInteractableType::None;
	float              NearestDist = TNumericLimits<float>::Max();
	const FVector      MyLoc = GetActorLocation();

	if (UHarvestManager* HMgr = MyGameInstance->GetHarvestManager())
	{
		const float R = HMgr->GetMaxHarvestDistance();

		if (ABasicMOB* M = HMgr->GetNearestHarvestableCorpse(R))
		{
			const float D = FVector::Dist(MyLoc, M->GetActorLocation());
			if (D < NearestDist) { Nearest = M; NearestType = EInteractableType::MOB_Harvestable; NearestDist = D; }
		}
	}

	if (InventoryManager)
	{
		const float R = CursorInteractionComponent ? CursorInteractionComponent->GetItemPickupRange() : 180.f;
		if (ADroppedItemActor* Item = InventoryManager->GetNearestDroppedItem(R))
		{
			const float D = FVector::Dist(MyLoc, Item->GetActorLocation());
			if (D < NearestDist)
			{
				Nearest = Item; NearestType = EInteractableType::DroppedItem; NearestDist = D;
			}
		}
	}

	if (UHarvestManager* HMgr = MyGameInstance->GetHarvestManager())
	{
		const float R = HMgr->GetMaxHarvestDistance();
		if (ABasicMOB* M = HMgr->GetNearestHarvestedCorpse(R))
		{
			const float D = FVector::Dist(MyLoc, M->GetActorLocation());
			if (D < NearestDist) { Nearest = M; NearestType = EInteractableType::MOB_Harvested; NearestDist = D; }
		}
	}

	{
		const float R = CursorInteractionComponent ? CursorInteractionComponent->GetItemPickupRange() : 180.f;
		TArray<AActor*> FoundItems;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADroppedItemActor::StaticClass(), FoundItems);
		for (AActor* Actor : FoundItems)
		{
			ADroppedItemActor* Item = Cast<ADroppedItemActor>(Actor);
			if (Item && Item->CanBePickedUp())
			{
				const float D = FVector::Dist(MyLoc, Item->GetActorLocation());
				if (D <= Item->GetPickupRadius() && D < NearestDist)
				{
					Nearest = Item; NearestType = EInteractableType::DroppedItem; NearestDist = D;
				}
			}
		}
	}

	{
		const float R = 250.f;
		for (TActorIterator<ABasicNPC> It(GetWorld()); It; ++It)
		{
			if (ABasicNPC* NPC = *It)
			{
				const float D = FVector::Dist(MyLoc, NPC->GetActorLocation());
				if (D <= R && D < NearestDist)
				{
					Nearest = NPC; NearestType = EInteractableType::NPC; NearestDist = D;
				}
			}
		}
	}

	if (InventoryManager)
	{
		const float R = CursorInteractionComponent ? CursorInteractionComponent->GetItemPickupRange() : 180.f;
		if (ADroppedItemActor* Item = InventoryManager->GetNearestDroppedItem(R))
		{
			const float D = FVector::Dist(MyLoc, Item->GetActorLocation());
			if (D < NearestDist)
			{
				Nearest = Item; NearestType = EInteractableType::DroppedItem; NearestDist = D;
			}
		}
	}

	if (Nearest && NearestType != EInteractableType::None)
	{
		UIManager->ShowProximityHint(NearestType);
	}
	else
	{
		UIManager->ClearProximityHint();
	}
}

void ABasicPlayer::DispatchPendingInteraction()
{
    AActor* Target = PendingInteractionTarget.Get();

    // Reset state first so re-entrant calls don't loop
    const EPendingInteraction Pending = PendingInteraction;
    PendingInteraction       = EPendingInteraction::None;
    PendingInteractionTarget = nullptr;
    PendingSkillSlug.Empty();

    if (!Target) return;

    switch (Pending)
    {
    case EPendingInteraction::AutoAttack:
        bIsAutoAttacking = true;
        DoAutoAttack();
        break;

    case EPendingInteraction::Harvest:
        {
            UHarvestManager* HMgr = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;
            if (HMgr)
                HMgr->TryHarvestSpecificCorpse(Cast<ABasicMOB>(Target));
        }
        break;

    case EPendingInteraction::TalkNPC:
        {
            UDialogueManager* DlgMgr = MyGameInstance ? MyGameInstance->GetDialogueManager() : nullptr;
            if (DlgMgr)
            {
                if (ABasicNPC* NPC = Cast<ABasicNPC>(Target))
                    DlgMgr->OpenDialogue(NPC->GetNPCId());
            }
        }
        break;

    case EPendingInteraction::PickupItem:
        if (InventoryManager)
            InventoryManager->PickupSpecificItem(Cast<ADroppedItemActor>(Target));
        break;

    default:
        break;
    }
}

void ABasicPlayer::HandleWeightStatusChanged(const FWeightStatusData& WeightStatus){
if (WeightStatus.isOverweight != bCachedOverweight)
{
bCachedOverweight = WeightStatus.isOverweight;
if (UIManager)
{
if (UNotificationToastWidget* Toast = UIManager->GetNotificationToastWidget())
{
FWorldNotificationStruct OverNotif;
OverNotif.notificationType = bCachedOverweight ? TEXT("overweight_started") : TEXT("overweight_ended");
OverNotif.channel = TEXT("toast");
OverNotif.dataFields.Emplace(TEXT("currentWeight"), FString::SanitizeFloat(WeightStatus.currentWeight));
OverNotif.dataFields.Emplace(TEXT("weightLimit"), FString::SanitizeFloat(WeightStatus.weightLimit));
Toast->ShowNotification(OverNotif);
}
}
}
    // Weight status is informational only (used by UI weight bar).
    // Movement speed is fully server-authoritative and arrives via move_speed in stats_update.
    // No client-side speed penalty is applied here.
    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Weight status updated вЂ” current=%.1f limit=%.1f overweight=%d (speed unchanged)"),
        WeightStatus.currentWeight, WeightStatus.weightLimit, (int)WeightStatus.isOverweight);
}

