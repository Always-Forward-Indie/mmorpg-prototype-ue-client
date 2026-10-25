#include "DevMode/DevModeBotBrain.h"
#include "Prototyping.h"
#include "DevMode/DevModeConsoleCommands.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Services/LocalizationSubsystem.h"
#include "Audio/AudioManager.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "UI/UIManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HighResScreenshot.h"
#include "Misc/Parse.h"
#include "DevMode/BotBrainHelpers.h"

bool UDevModeBotBrain::Tick_EquipOn(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UEquipmentManager* EqMgr = GameInstance->GetEquipmentManager();
        if (!EqMgr)
        {
            Finish(false, TEXT("no-equipment"));
            return true;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        const FEquipmentSlotData Slot = EqMgr->GetSlot(EquipSlotSlug);
        if (Slot.bIsOccupied && EquipInstanceId <= 0)
        {
            // Idempotent entry: already equipped (e.g. a previous run left
            // it on) — verify slot + visual, then continue the round-trip.
            bWasPreEquipped = true;
            EquipInstanceId = Slot.inventoryItemId;
            EquipItemSlug = Slot.itemSlug;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT slot %s pre-equipped instance=%d, verifying"),
                *EquipSlotSlug, EquipInstanceId);
        }
        if (EquipInstanceId <= 0)
        {
            EquipInstanceId = FindEquippableMainHand(EquipItemSlug);
            if (EquipInstanceId <= 0)
            {
                // The inventory snapshot is an async server push (lossy on
                // the shared stand — proven live 22.10: a whole run with
                // zero getPlayerInventory). An EMPTY inventory means "not
                // arrived yet": re-request throttled, wait the leg budget.
                // A non-empty inventory with nothing equippable is genuine.
                if (CountInventoryItems() <= 0)
                {
                    if (UInventoryManager* Inv = GameInstance->GetInventoryManager())
                    {
                        if (Now - LastInvResyncTime > 15.0)
                        {
                            LastInvResyncTime = Now;
                            Inv->RequestInventoryData(GameInstance->GetCurrentCharacterID());
                            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT no inventory yet, re-requesting"));
                        }
                    }
                    if (Now - CharLegStartTime > CharTimeoutSec)
                    {
                        Finish(false, TEXT("no-inventory-snapshot"));
                    }
                    return true;
                }
                Finish(false, TEXT("no-equippable"));
                return true;
            }
            EqMgr->RequestEquipItem(CharId, EquipInstanceId);
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT equip requested instance=%d '%s'"),
                EquipInstanceId, *EquipItemSlug);
            return true;
        }
        if (Slot.bIsOccupied && Slot.inventoryItemId == EquipInstanceId)
        {
            BotAssert(true, TEXT("equip-slot"), 1, 1);
            // Visual proof: the mesh the game attached for this slot.
            bool bVisualOk = false;
            if (UEquipmentVisualComponent* Vis = Player->GetEquipmentVisualComponent())
            {
                bVisualOk = Vis->GetItemSlugForSlot(EquipSlotSlug) == EquipItemSlug
                    && Vis->GetSlotComponent(EquipSlotSlug) != nullptr;
            }
            BotAssert(bVisualOk, TEXT("equip-visual"), bVisualOk ? 1 : 0, 1);
            BotShot(TEXT("equipped"));
            if (bRestorePass)
            {
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT pre-equipped item restored"));
                Finish(Failures == 0, Failures == 0 ? TEXT("equip-done") : TEXT("assert-failures"));
                return true;
            }
            Step = EBotStep::EquipOff;
            CharLegStartTime = Now;
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("equip-slot"), 0, EquipInstanceId);
            Finish(false, TEXT("equip-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_EquipOff(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UEquipmentManager* EqMgr = GameInstance->GetEquipmentManager();
        if (!EqMgr)
        {
            Finish(false, TEXT("no-equipment"));
            return true;
        }
        if (!bUnequipSent)
        {
            EqMgr->RequestUnequipItem(GameInstance->GetCurrentCharacterID(), EquipSlotSlug);
            bUnequipSent = true;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT unequip requested slot=%s"), *EquipSlotSlug);
            return true;
        }
        const FEquipmentSlotData Slot = EqMgr->GetSlot(EquipSlotSlug);
        if (!Slot.bIsOccupied)
        {
            BotAssert(true, TEXT("unequip-slot"), 1, 1);
            BotShot(TEXT("unequipped"));
            if (bWasPreEquipped)
            {
                // Restore the pre-run state, verify once, then finish.
                EqMgr->RequestEquipItem(GameInstance->GetCurrentCharacterID(), EquipInstanceId);
                bWasPreEquipped = false;
                bUnequipSent = false;
                bRestorePass = true;
                EquipInstanceId = 0;
                Step = EBotStep::EquipOn;
                CharLegStartTime = Now;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT restoring pre-equipped item"));
                return true;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("equip-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("unequip-slot"), 1, 0);
            Finish(false, TEXT("unequip-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_LevelWait(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // XP is granted externally mid-run (admin grant_xp): the bot only
        // polls the authoritative progression snapshot.
        const int32 LevelNow = GetProgressionLevel();
        const int32 ExpNow = GetProgressionExp();
        if (LevelNow > StartLevel && ExpNow > StartExp)
        {
            BotAssert(true, TEXT("level-grew"), LevelNow, StartLevel + 1);
            BotAssert(true, TEXT("exp-grew"), 1, 1);
            BotShot(TEXT("levelup"));
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT leveled: %d -> %d exp %d -> %d"),
                StartLevel, LevelNow, StartExp, ExpNow);
            Finish(Failures == 0, Failures == 0 ? TEXT("level-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("level-grew"), LevelNow, StartLevel + 1);
            Finish(false, TEXT("level-timeout"));
        }
        return true;
}
