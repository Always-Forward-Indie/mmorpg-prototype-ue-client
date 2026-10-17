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

//Create HUD
void ABasicPlayer::CreateHUD()
{
    //RefreshHUD();
}

void ABasicPlayer::HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience)
{
    if (playerData.isOtherClient) return;

    UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Level up! %d -> %d"), OldLevel, NewLevel);
    if (const FEntityAudioProfile* Profile = GetAudioProfile())
    {
        PlayEventSound(Profile->LevelUp);
    }
}

void ABasicPlayer::HandleEffectTickFCT(const FEffectTickData& TickData)
{
    // Only process ticks that belong to this specific player actor.
    if (TickData.characterId != playerData.characterData.characterId) return;

    if (!UIManager) return;
    UFloatingCombatTextManager* FCT = UIManager->GetFCTManager();
    if (!FCT) return;

    const FVector Pos = GetCombatPosition_Implementation();

    if (TickData.bIsHeal && TickData.value > 0)
    {
        // Heal-over-time → green "+N"
        ShowHealingEffect_Implementation(TickData.value, TickData.effectSlug);
    }
    else if (!TickData.bIsHeal && TickData.value > 0)
    {
        // Damage-over-time → damage-coloured number
        EDamageType DmgType = EDamageType::Physical;
        if      (TickData.effectTypeSlug.Contains(TEXT("poison"))) DmgType = EDamageType::Poison;
        else if (TickData.effectTypeSlug.Contains(TEXT("fire")) || TickData.effectTypeSlug.Contains(TEXT("burn"))) DmgType = EDamageType::Fire;
        else if (TickData.effectTypeSlug.Contains(TEXT("ice"))  || TickData.effectTypeSlug.Contains(TEXT("frost"))) DmgType = EDamageType::Ice;
        FCT->ShowDamage(Pos, static_cast<float>(TickData.value), false, DmgType, true);
    }
}

void ABasicPlayer::HandleChatMessageForBubble(const FChatMessageStruct& Message)
{
    // Ignore error / system messages.
    if (Message.bIsError) return;
    if (Message.senderId <= 0) return;

    // Find the player actor who sent this message and show their speech bubble.
    // Only remote players are registered in NameplateManager, so the local player's
    // own messages are silently skipped (no nameplate entry for bIsLocalPlayer actors).
    if (!MyGameInstance) return;

    ABasicPlayer* Sender = MyGameInstance->GetPlayerByCharacterId(Message.senderId);
    if (!Sender || !Sender->GetNameplateComponent()) return;

    Sender->GetNameplateComponent()->ShowChatBubble(Message.text, ChatBubbleDisplayDuration);
}

void ABasicPlayer::HandleTitlesUpdated(const FPlayerTitlesState& State)
{
    // Push the currently-equipped title name to this actor's nameplate.
    // Resolve the localized FText from the slug first; fall back to server-provided name.
    if (NameplateComponent)
    {
        FText TitleText;
        if (!State.equippedTitle.slug.IsEmpty() && MyGameInstance)
        {
            if (const ULocalizationSubsystem* Loc = MyGameInstance->GetSubsystem<ULocalizationSubsystem>())
            {
                TitleText = Loc->GetTitleDisplayName(State.equippedTitle.slug);
            }
        }
        if (TitleText.IsEmpty() && !State.equippedTitle.displayName.IsEmpty())
        {
            TitleText = FText::FromString(State.equippedTitle.displayName);
        }
        NameplateComponent->UpdateTitle(TitleText);
    }
}

void ABasicPlayer::SetEquippedTitle(const FText& TitleDisplayName)
{
    if (NameplateComponent)
    {
        NameplateComponent->UpdateTitle(TitleDisplayName);
    }
}

void ABasicPlayer::HandleUIManagerInitialized()
{
    // Must fire exactly once per spawn: the fallback timer can race the Slate
    // NativeTick broadcast, and without this guard cursor delegates would bind
    // twice (double click dispatch).
    if (bUIInitDone) { return; }
    if (UIManager)
    {
        UIManager->OnUIManagerInitialized.RemoveDynamic(this, &ABasicPlayer::HandleUIManagerInitialized);
    }

    // At this point PlayerInterfaceWidget->AddToViewport() was called one game-thread
    // tick ago, so the render thread has already received the draw command for the
    // game UI. It is now safe to signal the loading-screen countdown gates.
    bUIInitDone = true;

    if (MyGameInstance)
    {
        MyGameInstance->NotifyUIInitialized();

        // Start idle timeout tracking now that all UI widgets are ready
        if (MyGameInstance->IdleTimeoutManager)
        {
            APlayerController* PC = Cast<APlayerController>(GetController());
            if (PC)
            {
                MyGameInstance->IdleTimeoutManager->StartTracking(PC, UIManager);
            }
        }
    }

    // Bind cursor interaction delegates now that UI is ready.
    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->OnSingleClicked.AddDynamic(
            this, &ABasicPlayer::DispatchCursorSelect);
        CursorInteractionComponent->OnDoubleClicked.AddDynamic(
            this, &ABasicPlayer::DispatchCursorInteract);
        CursorInteractionComponent->OnHoverChanged.AddDynamic(
            this, &ABasicPlayer::HandleInteractiveHoverChanged);
    }

    // Subscribe to WIO actor spawns so we can track proximity changes
    if (MyGameInstance)
    {
        if (UWorldObjectManager* WOM = MyGameInstance->GetWorldObjectManager())
        {
            WOM->OnWorldObjectActorSpawned.AddDynamic(this, &ABasicPlayer::HandleWIOActorSpawned);

            // Also bind existing actors (in case they spawned before UI was ready)
            for (AWorldInteractiveObjectActor* Actor : WOM->GetAllObjectActors())
            {
                if (Actor)
                {
                    Actor->OnProximityChanged.AddDynamic(this, &ABasicPlayer::HandleWIOProximityChanged);
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] HandleUIManagerInitialized: bUIInitDone=true, NotifyUIInitialized sent. Pos=(%.0f,%.0f,%.0f)"),
        GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}


// update HUD
void ABasicPlayer::UpdateHUD()
{
    if (PlayerHUD)
    {
        if (!GetIsOtherClient())
        {
            // Only update HP/MP bars when we have real server-authoritative max values
            // (populated by the first stats_update). Before that keep the bars hidden/empty
            // so we don't show a fake full HP bar on join.
            const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health"));
            const FAttributeDataStruct* ManaAttr   = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana"));

            if (HealthAttr && HealthAttr->attributeValue > 0 &&
                ManaAttr   && ManaAttr->attributeValue   >= 0)
            {
                PlayerHUD->SetHP(playerData.characterData.characterCurrentHealth,
                                 static_cast<float>(HealthAttr->attributeValue));
                PlayerHUD->SetMana(playerData.characterData.characterCurrentMana,
                                   static_cast<float>(ManaAttr->attributeValue));
            }
            // else: bars remain at their last known state until stats_update arrives
        }
    }
}

