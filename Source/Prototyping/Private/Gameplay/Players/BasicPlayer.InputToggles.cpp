// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "Prototyping.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
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
#include "Utils/NetCore/TargetingRule.h"
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

void ABasicPlayer::OnTabTargetInput()
{
    if (playerData.characterData.bIsDead || playerData.isOtherClient) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
    const FVector CameraForward = CameraRotation.Vector();

    const float MaxTabRange = CursorInteractionComponent ? CursorInteractionComponent->GetTabTargetRange() : 1500.0f;
    const FVector PlayerLoc = GetActorLocation();

    // AAA Tab: only mobs in the direction the camera looks (front hemisphere),
    // ordered by angle to the crosshair (nearest first), distance as tiebreak.
    // A stale/off-screen lock never seeds the cycle: turning away restarts
    // from the best target in the current view (FTargetingRule::TabStartIndex).
    struct FTabEntry
    {
        ABasicMOB* Mob = nullptr;
        float Dot = 0.0f;
        float DistSq = 0.0f;
    };
    TArray<FTabEntry> Visible;
    // Registry iteration (O(registered mobs), no world scan) — consistent with CheckForMOB.
    if (UMOBManager* MobMgr = MyGameInstance ? MyGameInstance->GetMOBManager() : nullptr)
    {
        MobMgr->ForEachMob([&](ABasicMOB* Mob)
        {
            if (!Mob || Mob->GetMOBIsDead()) return;
            const float Dist = FVector::Dist(PlayerLoc, Mob->GetActorLocation());
            if (!FTargetingRule::IsInRange(Dist, MaxTabRange)) return;
            const FVector ToMob = (Mob->GetActorLocation() - CameraLocation).GetSafeNormal();
            const float Dot = FVector::DotProduct(CameraForward, ToMob);
            if (!FTargetingRule::IsInFront(Dot)) return;
            // No targeting through walls (discrete key press — traces are cheap here).
            FHitResult LOSHit;
            FCollisionQueryParams LOSParams;
            LOSParams.AddIgnoredActor(this);
            LOSParams.AddIgnoredActor(Mob);
            if (GetWorld()->LineTraceSingleByChannel(LOSHit, CameraLocation,
                Mob->GetActorLocation(), ECC_Visibility, LOSParams)) return;
            Visible.Add({ Mob, Dot, Dist * Dist });
        });
    }

    if (Visible.Num() == 0)
    {
        ClearLockedTarget();
        return;
    }

    Visible.Sort([](const FTabEntry& A, const FTabEntry& B)
    {
        return FTargetingRule::TabCandidateLess(A.Dot, A.DistSq, B.Dot, B.DistSq);
    });

    int32 LockedIdx = INDEX_NONE;
    if (IsValid(LockedTarget))
    {
        for (int32 i = 0; i < Visible.Num(); ++i)
        {
            if (Visible[i].Mob == LockedTarget)
            {
                LockedIdx = i;
                break;
            }
        }
    }

    SetLockedTarget(Visible[FTargetingRule::TabStartIndex(LockedIdx, Visible.Num())].Mob);
}

void ABasicPlayer::LockMovementForPickup()
{
    if (bIsPickingUp) return;
    bIsPickingUp = true;

    // Stop any active approach/auto-attack so the player stands still
    StopAutoAttack();

    LockMovement();
    UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Movement locked for pickup"));

    // Safety timeout: unlock movement after 5s even if the pickup chain fails
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(PickupSafetyTimerHandle, this,
            &ABasicPlayer::UnlockMovementAfterPickup, 5.0f, false);
    }
}

void ABasicPlayer::UnlockMovementAfterPickup()
{
    if (!bIsPickingUp) return;
    bIsPickingUp = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PickupSafetyTimerHandle);
    }

    UnlockMovement();
    UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Movement unlocked after pickup"));
}

void ABasicPlayer::LockMovement()
{
    // Corpses don't lock: input is already gated by bIsDead, and there is
    // no live mode to preserve. Capturing the corpse's MOVE_None here would
    // make UnlockMovement restore it AFTER revive (no gravity, no
    // translation) — the teleport cover flow relies on this for respawns.
    if (playerData.characterData.bIsDead)
    {
        return;
    }
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        SavedMovementMode = MoveComp->MovementMode;

        // Drain pending movement input so no residual acceleration carries over
        ConsumeMovementInputVector();

        // Zero horizontal velocity to stop any slide/drift immediately,
        // but preserve Z so gravity-affected falling continues uninterrupted.
        FVector Vel = MoveComp->Velocity;
        Vel.X = 0.0f;
        Vel.Y = 0.0f;
        MoveComp->Velocity = Vel;
        MoveComp->UpdateComponentVelocity();
    }
    bMovementLocked = true;
}

void ABasicPlayer::UnlockMovement()
{
    bMovementLocked = false;
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        if (MoveComp->MovementMode != SavedMovementMode)
        {
            MoveComp->SetMovementMode(SavedMovementMode);
        }
    }
}

void ABasicPlayer::OnInventoryToggle()
{
    UE_LOG(LogGameplay, Warning, TEXT("Inventory toggle pressed"));
    
    if (InventoryManager)
    {
        InventoryManager->ToggleInventoryUI();
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("Inventory manager not found"));
    }
}

void ABasicPlayer::OnSkillsPanelToggle()
{
    UE_LOG(LogGameplay, Warning, TEXT("Skills panel toggle pressed"));
    
    if (UIManager)
    {
        UIManager->ToggleSkillsPanel();
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("UI manager not found"));
    }
}

void ABasicPlayer::OnGameMenuToggle()
{
    // If currently channeling a WIO, Escape cancels the channel instead of opening the menu
    if (MyGameInstance)
    {
        UWorldObjectManager* WOM = MyGameInstance->GetWorldObjectManager();
        if (WOM && WOM->IsChanneling())
        {
            CancelWIOChannelIfActive();
            return;
        }
    }

    if (UIManager)
    {
        UIManager->ToggleGameMenu();
    }
}

void ABasicPlayer::OnSkill1Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(0) : TEXT("");
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(0);
    }
}

void ABasicPlayer::OnSkill2Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(1) : TEXT("");
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(1);
    }
}

void ABasicPlayer::OnSkill3Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(2) : TEXT("");
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(2);
    }
}

void ABasicPlayer::OnSkill4Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(3) : TEXT("");
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(3);
    }
}

void ABasicPlayer::OnSkill5Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(4) : TEXT("");
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(4);
    }
}

void ABasicPlayer::UseQuickBarSlot(int32 SlotIndex)
{
    if (playerData.characterData.bIsDead) return;
    if (!UIManager) return;
    UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget();
    if (!PIW) return;
    UItemQuickBarWidget* QuickBar = PIW->GetItemQuickBar();
    if (!QuickBar) return;
    QuickBar->UseSlot(SlotIndex);
}

void ABasicPlayer::OnQuickSlot1Input() { UseQuickBarSlot(0); }
void ABasicPlayer::OnQuickSlot2Input() { UseQuickBarSlot(1); }
void ABasicPlayer::OnQuickSlot3Input() { UseQuickBarSlot(2); }
void ABasicPlayer::OnQuickSlot4Input() { UseQuickBarSlot(3); }
void ABasicPlayer::OnQuickSlot5Input() { UseQuickBarSlot(4); }

void ABasicPlayer::OnKeybindsToggle()
{
    if (playerData.characterData.bIsDead) return;
    if (UIManager)
        UIManager->ToggleKeybindWindow();
}

void ABasicPlayer::GetRemappableActions(TArray<FKeybindActionEntry>& OutActions) const
{
    auto Add = [&OutActions](const FText& Label, UInputAction* Action)
    {
        if (!Action) return;
        FKeybindActionEntry E;
        E.Label = Label;
        E.Action = Action;
        OutActions.Add(E);
    };

    Add(NSLOCTEXT("Keybinds", "Move", "Move"), MoveAction);
    Add(NSLOCTEXT("Keybinds", "Jump", "Jump"), JumpAction);
    Add(NSLOCTEXT("Keybinds", "Attack", "Attack"), AttackAction);
    Add(NSLOCTEXT("Keybinds", "Interact", "Interact"), InteractAction);
    Add(NSLOCTEXT("Keybinds", "Inventory", "Inventory"), InventoryAction);
    Add(NSLOCTEXT("Keybinds", "SkillsPanel", "Skills panel"), SkillsPanelAction);
    Add(NSLOCTEXT("Keybinds", "Skill1", "Skill 1"), Skill1Action);
    Add(NSLOCTEXT("Keybinds", "Skill2", "Skill 2"), Skill2Action);
    Add(NSLOCTEXT("Keybinds", "Skill3", "Skill 3"), Skill3Action);
    Add(NSLOCTEXT("Keybinds", "Skill4", "Skill 4"), Skill4Action);
    Add(NSLOCTEXT("Keybinds", "Skill5", "Skill 5"), Skill5Action);
    Add(NSLOCTEXT("Keybinds", "QuickSlot1", "Quick slot 1"), QuickSlot1Action);
    Add(NSLOCTEXT("Keybinds", "QuickSlot2", "Quick slot 2"), QuickSlot2Action);
    Add(NSLOCTEXT("Keybinds", "QuickSlot3", "Quick slot 3"), QuickSlot3Action);
    Add(NSLOCTEXT("Keybinds", "QuickSlot4", "Quick slot 4"), QuickSlot4Action);
    Add(NSLOCTEXT("Keybinds", "QuickSlot5", "Quick slot 5"), QuickSlot5Action);
    Add(NSLOCTEXT("Keybinds", "TabTarget", "Tab target"), TabTargetAction);
    Add(NSLOCTEXT("Keybinds", "TargetSelf", "Target self"), TargetSelfAction);
    Add(NSLOCTEXT("Keybinds", "QuestJournal", "Quest journal"), QuestJournalAction);
    Add(NSLOCTEXT("Keybinds", "Equipment", "Equipment"), EquipmentAction);
    Add(NSLOCTEXT("Keybinds", "AltCursor", "Alt cursor"), AltCursorAction);
    Add(NSLOCTEXT("Keybinds", "Stats", "Stats"), StatsAction);
    Add(NSLOCTEXT("Keybinds", "Bestiary", "Bestiary"), BestiaryAction);
    Add(NSLOCTEXT("Keybinds", "Titles", "Titles"), TitlesAction);
    Add(NSLOCTEXT("Keybinds", "Reputation", "Reputation"), ReputationAction);
    Add(NSLOCTEXT("Keybinds", "Emotes", "Emotes"), EmoteListAction);
    Add(NSLOCTEXT("Keybinds", "GameMenu", "Game menu"), GameMenuAction);
}

