// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "Prototyping.h"
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

// Add these method implementations at the end of the file, before the closing brace

void ABasicPlayer::UpdatePlayerStats(const FPlayerStatsUpdateStruct& StatsUpdate)
{
	// Validate the stats update
	if (!PlayerAttributeParser::ValidateStatsData(StatsUpdate))
	{
		UE_LOG(LogGameplay, Error, TEXT("BasicPlayer: Invalid stats update data received"));
		return;
	}

	// Check if this update is for this player
	if (StatsUpdate.characterId != playerData.characterData.characterId)
	{
		UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Stats update for different character (Expected: %d, Received: %d)"), 
			playerData.characterData.characterId, StatsUpdate.characterId);
		return;
	}

	// Update character data
	PlayerAttributeParser::UpdateCharacterDataFromStatsUpdate(playerData.characterData, StatsUpdate);

	UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer: Updated stats for character %d - Level: %d, HP: %d/%d, MP: %d/%d"),
		StatsUpdate.characterId, StatsUpdate.level,
		StatsUpdate.healthCurrent, StatsUpdate.healthMax,
		StatsUpdate.manaCurrent, StatsUpdate.manaMax);
}

void ABasicPlayer::ApplyServerMoveSpeed(float ServerMoveSpeed)
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		const float NewSpeed = ServerMoveSpeed * MoveSpeedScale;
		CMC->MaxWalkSpeed = NewSpeed;
		UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer: MaxWalkSpeed set to %.1f (server move_speed=%.1f, scale=%.1f)"),
			NewSpeed, ServerMoveSpeed, MoveSpeedScale);
	}
}

void ABasicPlayer::ApplyVisualFromDataTable(UDataTable* VisualTable)
{
	if (!VisualTable) return;

	const FString& ClassSlug  = playerData.characterData.characterClass;
	const FString& RaceSlug   = playerData.characterData.characterRace;
	const FString& GenderName = playerData.characterData.characterGender;

	if (GenderName.IsEmpty())
	{
		UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer::ApplyVisualFromDataTable: characterGender is empty for charId=%d — server may not be sending 'gender' in joinGameCharacter. Falling back through visual definition chain."),
			playerData.characterData.characterId);
	}

	const FCharacterVisualDefinition* Def = CharacterVisualHelper::FindVisualDefinition(
		VisualTable, ClassSlug, RaceSlug, GenderName);

	if (!Def)
	{
		UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer::ApplyVisualFromDataTable: No definition for %s_%s_%s"),
			*ClassSlug, *RaceSlug, *GenderName);
		return;
	}

	const FCharacterVisualData& VisualData = Def->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TSoftObjectPtr<USkeletalMesh> SoftMesh = VisualData.SkeletalMesh;
	TSoftClassPtr<UAnimInstance>  SoftAnimBP = VisualData.AnimBPClass;

	TWeakObjectPtr<ABasicPlayer> WeakThis(this);

	if (!SoftMesh.IsNull())
	{
		Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath(), [WeakThis, SoftMesh]()
		{
			if (!WeakThis.IsValid()) return;
			ABasicPlayer* Self = WeakThis.Get();
			if (USkeletalMesh* LoadedMesh = SoftMesh.Get())
			{
				USkeletalMeshComponent* MeshComp = Self->GetMesh();
				if (MeshComp)
				{
					MeshComp->SetSkeletalMesh(LoadedMesh);
					MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

					const FBoxSphereBounds MeshBounds = LoadedMesh->GetBounds();
					const FVector BoxExtent = MeshBounds.BoxExtent;
					const FVector MeshOrigin = MeshBounds.Origin;

					UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
					if (Capsule)
					{
						float CapsuleRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
						float CapsuleHalfHeight = BoxExtent.Z;

						Capsule->SetCapsuleRadius(CapsuleRadius/2);
						Capsule->SetCapsuleHalfHeight(CapsuleHalfHeight);

						float MeshBottom = MeshOrigin.Z - BoxExtent.Z;
						float CapsuleBottom = -Capsule->GetUnscaledCapsuleHalfHeight();
						float MeshOffset = CapsuleBottom - MeshBottom;
						MeshComp->SetRelativeLocation(FVector(0, 0, MeshOffset));
						MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
					}
				}
			}
		});
	}

	if (!SoftAnimBP.IsNull())
	{
		Streamable.RequestAsyncLoad(SoftAnimBP.ToSoftObjectPath(), [WeakThis, SoftAnimBP]()
		{
			if (!WeakThis.IsValid()) return;
			if (UClass* AnimClass = SoftAnimBP.Get())
			{
				WeakThis->GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		});
	}

	SetActorScale3D(VisualData.ActorScale);

	if (Def->AudioProfileId != NAME_None)
	{
		AudioProfileId = Def->AudioProfileId;
	}

	if (const FEntityAudioProfile* Prof = GetAudioProfile())
	{
		if (!Prof->FootwearType.IsNone())
		{
			FootwearType = Prof->FootwearType;
		}
	}

	PreloadFootstepSounds();

	// Initialize default cosmetics
	// This must happen after SetActorScale3D so the body component pointer is stable.
	UE_LOG(LogGameplay, Verbose, TEXT("[Cosmetic] ApplyVisualFromDataTable: CosmeticVisualComponent=%s  MyGameInstance=%s  CosmeticsTable=%s"),
		CosmeticVisualComponent ? TEXT("OK") : TEXT("NULL — not created in constructor?"),
		MyGameInstance ? TEXT("OK") : TEXT("NULL"),
		(MyGameInstance && MyGameInstance->CharacterCosmeticsDataTable) ? *MyGameInstance->CharacterCosmeticsDataTable->GetName() : TEXT("NULL — assign in BP_GameInstance"));
	if (CosmeticVisualComponent)
	{
		UDataTable* CosmeticsTable = MyGameInstance ? MyGameInstance->CharacterCosmeticsDataTable : nullptr;
		CosmeticVisualComponent->SetDefaultCosmetics(*Def, CosmeticsTable);
	}

	UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer::ApplyVisualFromDataTable: Applied visual for %s_%s_%s"),
		*ClassSlug, *RaceSlug, *GenderName);
}

void ABasicPlayer::ProcessStatsUpdate(const FPlayerStatsUpdateStruct& StatsUpdate)
{
	// Capture old level BEFORE UpdatePlayerStats overwrites characterData
	const int32 OldLevel = playerData.characterData.characterLevel;

	// Capture pre-update HP/MP so HandleStatsManagerUpdate can compute accurate deltas.
	// Without this, the first handler (ProcessStatsUpdate) overwrites characterCurrentHealth
	// before HandleStatsManagerUpdate fires, making OldHP == NewHP every time → gain=0.
	LastPreUpdateHP = playerData.characterData.characterCurrentHealth;
	LastPreUpdateMP = playerData.characterData.characterCurrentMana;

	// Guard: if the player is already dead and the incoming packet carries positive HP
	// (a stale regen or damage snapshot that was queued before the kill was processed),
	// skip the HP/MP update to prevent the HUD showing rising HP on a corpse.
	// stats_update(healthCurrent=0) packets must still pass through so combat death
	// (detected below) and the loading-screen gate (NotifyStatsReceived) can fire.
	if (!playerData.characterData.bIsDead || StatsUpdate.healthCurrent == 0)
	{
		// Update HP/MP/level on the character data
		UpdatePlayerStats(StatsUpdate);
	}

	if (playerData.isOtherClient && NameplateComponent)
	{
		NameplateComponent->UpdateHealth(StatsUpdate.healthCurrent, StatsUpdate.healthMax);
		NameplateComponent->UpdateLevel(StatsUpdate.level);
	}

	// Refresh the HP/MP HUD
	RefreshHUD();

	// Signal the loading screen gate: HUD has real server data.
	// GameInstance guards against double-firing internally.
	if (!playerData.isOtherClient && MyGameInstance)
	{
		MyGameInstance->NotifyStatsReceived();
	}

	// --- Death detection (#1) ---
	// The server signals death via stats_update with health.current == 0.
	// Transition into dead state only if we are not already dead to avoid
	// re-triggering sounds / anim notifications on every repeated packet.
	// Guard: a respawn grace window of 5 s prevents a stale stats_update snapshot
	// (sent by the server at the moment of death, before it processed the respawn)
	// from re-killing the player immediately after they stand up.
	if (StatsUpdate.healthCurrent == 0 && !playerData.characterData.bIsDead)
	{
		const float TimeSinceRespawn = GetWorld()
			? (GetWorld()->GetTimeSeconds() - LastRespawnWorldTime)
			: 999.f;
		constexpr float RespawnGraceSeconds = 5.0f;
		if (TimeSinceRespawn >= RespawnGraceSeconds)
		{
			SetDead_Implementation(true);
		}
		else
		{
			UE_LOG(LogGameplay, Warning,
				TEXT("BasicPlayer: Ignoring stats_update(HP=0) — within respawn grace window (%.1fs < %.1fs)"),
				TimeSinceRespawn, RespawnGraceSeconds);
		}
	}

	// Sync experience fields only when the packet actually carries them.
	// Partial packets (e.g. regen ticks) omit the "experience" block so all
	// four fields arrive as 0 -- we must not overwrite the last known values.
	const bool bHasExpData = (StatsUpdate.experienceCurrent > 0
		|| StatsUpdate.experienceNextLevel  > 0
		|| StatsUpdate.experienceLevelStart > 0);
	if (bHasExpData)
	{
		playerData.characterData.characterExperiencePoints = StatsUpdate.experienceCurrent;
		playerData.characterData.characterExpForLevelStart = StatsUpdate.experienceLevelStart;
		playerData.characterData.characterExpForLevelEnd   = StatsUpdate.experienceNextLevel;
		playerData.characterData.characterExperienceDebt   = StatsUpdate.experienceDebt;
		UpdateExperienceData();
	}

	// Forward every stats_update to PlayerStatsManager so all subscribed widgets
	// (PlayerStatsWidget, PlayerExperienceWidget) receive the authoritative snapshot.
	if (MyGameInstance && !playerData.isOtherClient)
	{
		if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
		{
			StatsMgr->ApplyStatsUpdate(StatsUpdate);
		}
	}

	// Apply move_speed attribute from the server to the CharacterMovementComponent.
	// Server is authoritative: MaxWalkSpeed = move_speed * MoveSpeedScale, no client-side modifiers.
	if (!playerData.isOtherClient)
	{
		const FStatAttributeEntry* SpeedAttr = StatsUpdate.attributes.FindByPredicate(
			[](const FStatAttributeEntry& A){ return A.slug.Equals(TEXT("move_speed"), ESearchCase::IgnoreCase); });

		if (SpeedAttr && SpeedAttr->effective > 0.f)
		{
			ApplyServerMoveSpeed(SpeedAttr->effective);
		}
	}

	// Forward the full active-effects list to the HUD widget (replaces previous snapshot).
	// ActiveEffectsWidget keeps its own per-second tick for countdown labels.
	if (UIManager)
	{
		if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
		{
			if (UActiveEffectsWidget* AEW = PIW->GetActiveEffectsWidget())
			{
				AEW->RefreshEffects(StatsUpdate.activeEffects);
			}
		}
	}

	// Log each effect for diagnostics and fire ShowBuffEffect for Blueprint hooks.
	for (const FActiveEffectEntry& Effect : StatsUpdate.activeEffects)
	{
		FAppliedEffectData EffectData;
		EffectData.effectName = Effect.slug;
		EffectData.effectType = Effect.effectTypeSlug;
		if (Effect.expiresAt > 0)
		{
			const int64 NowSec = static_cast<int64>(FDateTime::UtcNow().ToUnixTimestamp());
			EffectData.duration = static_cast<float>(FMath::Max<int64>(Effect.expiresAt - NowSec, 0));
		}
		EffectData.value = static_cast<int32>(Effect.value);
		ShowBuffEffect_Implementation(EffectData);
	}

	UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer: Processed stats update and refreshed UI"));
}

void ABasicPlayer::HandleStatsManagerUpdate(const FPlayerStatsUpdateStruct& NewStats)
{
	// Log BEFORE any early return so we can see if the function is ever called at all.
	UE_LOG(LogGameplay, Verbose, TEXT("[EFFECTS] HandleStatsManagerUpdate: ENTER charId=%d myCharId=%d isOtherClient=%d effects=%d"),
		NewStats.characterId, playerData.characterData.characterId, (int)playerData.isOtherClient, NewStats.activeEffects.Num());

	// Only handle updates for the local player and for our own character.
	if (playerData.isOtherClient) return;
	if (NewStats.characterId != playerData.characterData.characterId) return;

	// If the player is dead, don't update vitals — regen ticks from the server
	// would show HP refilling on the death screen and break the bIsDead contract.
	if (playerData.characterData.bIsDead) return;

	// Capture current vitals before the update so we can compute deltas.
	// Use LastPreUpdate values when available (saved by ProcessStatsUpdate before
	// it overwrote characterData). Consume them so the second broadcast from
	// PlayerStatsNetworkHandler doesn't duplicate the floating numbers.
	const int32 OldHP = (LastPreUpdateHP >= 0) ? LastPreUpdateHP : playerData.characterData.characterCurrentHealth;
	const int32 OldMP = (LastPreUpdateMP >= 0) ? LastPreUpdateMP : playerData.characterData.characterCurrentMana;
	LastPreUpdateHP = -1;
	LastPreUpdateMP = -1;
	const int32 NewHP = NewStats.healthCurrent;
	const int32 NewMP = NewStats.manaCurrent;

	// Use the authoritative parser so that max_health / max_mana attributes in
	// playerData are always kept in sync alongside the current vitals.
	PlayerAttributeParser::UpdateCharacterDataFromStatsUpdate(playerData.characterData, NewStats);

	RefreshHUD();

	// Show floating combat text for any positive HP/MP delta, excluding direct damage.
	if (UIManager)
	{
		if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
		{
			const int32 HPGain = NewHP - OldHP;
			const int32 MPGain = NewMP - OldMP;

			UE_LOG(LogGameplay, Verbose, TEXT("[REGEN] source=%s oldHP=%d newHP=%d gain=%d oldMP=%d newMP=%d gain=%d"),
				*NewStats.updateSource, OldHP, NewHP, HPGain, OldMP, NewMP, MPGain);

			const bool bIsDamage = NewStats.updateSource == TEXT("damage");

			if (HPGain > 0 && !bIsDamage)
				FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(HPGain), false, EDamageType::Heal, true);
			if (MPGain > 0 && !bIsDamage)
				FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(MPGain), false, EDamageType::ManaRegen, true);
		}
	}

	// Forward activeEffects to the HUD buff bar. This mirrors ProcessStatsUpdate's
	// behaviour and ensures effects are visible even when stats arrive before the
	// player actor is registered (when ProcessStatsUpdate cannot be called).
	UE_LOG(LogGameplay, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: PASS UIManager=%s effects=%d"),
		UIManager ? TEXT("valid") : TEXT("NULL"), NewStats.activeEffects.Num());

	if (UIManager)
	{
		if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
		{
			UActiveEffectsWidget* AEW = PIW->GetActiveEffectsWidget();
			UE_LOG(LogGameplay, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: PIW=valid AEW=%s effects=%d"),
				AEW ? TEXT("valid") : TEXT("NULL"), NewStats.activeEffects.Num());
			if (AEW)
			{
				AEW->RefreshEffects(NewStats.activeEffects);
			}
		}
		else
		{
			UE_LOG(LogGameplay, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: PIW is NULL"));
		}
	}
}


void ABasicPlayer::RefreshHUD()
{
	if (!PlayerHUD)
	{
		UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Cannot refresh HUD - PlayerHUD is null"));
		return;
	}

	// Primary source: characterAttributes populated from joinGameCharacter (stats.health.max)
	// and kept up-to-date by every stats_update via UpdateCharacterDataFromStatsUpdate.
	const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health"));
	const FAttributeDataStruct* ManaAttr   = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana"));

	float MaxHP  = (HealthAttr && HealthAttr->attributeValue > 0) ? static_cast<float>(HealthAttr->attributeValue) : 0.f;
	float MaxMP  = (ManaAttr   && ManaAttr->attributeValue   > 0) ? static_cast<float>(ManaAttr->attributeValue)   : 0.f;

	// Fallback: if characterAttributes are still empty (race condition between spawn
	// and first stats_update), read from the PlayerStatsManager cache.
	if (MaxHP <= 0.f && MyGameInstance)
	{
		if (const UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
		{
			const FPlayerStatsUpdateStruct& Cached = StatsMgr->GetCachedStats();
			if (Cached.characterId == playerData.characterData.characterId)
			{
				MaxHP = static_cast<float>(Cached.healthMax);
				MaxMP = static_cast<float>(Cached.manaMax);
			}
		}
	}

	if (MaxHP > 0.f)
	{
		PlayerHUD->SetHP(static_cast<float>(playerData.characterData.characterCurrentHealth), MaxHP);

		if (UIManager)
		{
			const float HPRatio = static_cast<float>(playerData.characterData.characterCurrentHealth) / MaxHP;
			UIManager->SetLowHealthWarning(HPRatio > 0.0f && HPRatio <= 0.3f);
		}
	}

	if (MaxMP >= 0.f && (MaxHP > 0.f || MaxMP > 0.f))
	{
		PlayerHUD->SetMana(static_cast<float>(playerData.characterData.characterCurrentMana), MaxMP);
	}
}
