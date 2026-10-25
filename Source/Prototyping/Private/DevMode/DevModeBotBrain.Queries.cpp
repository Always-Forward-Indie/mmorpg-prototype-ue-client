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

void UDevModeBotBrain::BotAssert(bool bPass, const FString& What, int32 Actual, int32 Expected)
{
    if (bPass)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_ASSERT PASS bot-%s actual=%d"), *What, Actual);
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_ASSERT FAIL bot-%s actual=%d expected=%d"), *What, Actual, Expected);
        ++Failures;
    }
}

void UDevModeBotBrain::BotShot(const FString& Tag) const
{
    // Screenshots need a renderer; headless (-nullrhi) runs skip them.
    if (!FParse::Param(FCommandLine::Get(), TEXT("devmode-shots")))
    {
        return;
    }
    // Deterministic file name (no frame suffix) so runs overwrite and the
    // golden gate can match by exact name.
    const FString Name = FString::Printf(TEXT("DevBot_%s_%s"), *ScenarioName, *Tag);
    if (FScreenshotRequest::IsScreenshotRequested() || !PendingShotName.IsEmpty())
    {
        // The pipe holds ONE pending request: a same-tick burst would
        // silently eat the earlier shot (lost the death checkpoint this
        // way). Park it; the per-frame SteerTick pump issues it the moment
        // the pipe is free.
        PendingShotName = Name;
        UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_BOT_SHOT_QUEUED %s_%s"), *ScenarioName, *Tag);
        return;
    }
    FScreenshotRequest::RequestScreenshot(Name, true, false);
    UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_BOT_SHOT %s_%s"), *ScenarioName, *Tag);
}

void UDevModeBotBrain::Finish(bool bPass, const FString& Reason)
{
    if (!bRunning)
    {
        return;
    }
    // Last-chance shot flush: a parked BotShot (same-tick burst) would die
    // with the ticker below; the quit itself is deferred, so one framed
    // request still flushes (kill2 precedent). Only when the pipe is free —
    // issuing over a busy pipe overwrites the in-flight shot (same bug).
    if (!PendingShotName.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested())
    {
        FScreenshotRequest::RequestScreenshot(PendingShotName, true, false);
        UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_BOT_SHOT_FLUSHED %s (finish)"), *PendingShotName);
        PendingShotName.Empty();
    }
    const bool bOk = bPass && Failures == 0;
    Stop();
    UE_LOG(LogGameplay, Warning, TEXT("%s_RESULT %s %s kills=%d failures=%d reason=%s"),
        *TombstonePrefix, *ScenarioName, bOk ? TEXT("PASS") : TEXT("FAIL"), Kills, Failures, *Reason);

    // Auto-run paths (-devmode=<scenario>, -bot-live=<scenario>) quit so CI
    // gates on the tombstone; manual console runs stay in the world.
    if (bAutoQuitAtFinish || (GameInstance && GameInstance->bDevModeAutoQuit))
    {
        GameInstance->bDevModeAutoQuit = false;
        GameInstance->DevModeAutoScenario.Empty();
        if (UWorld* World = BrainWorld.Get())
        {
            if (GEngine)
            {
                GEngine->Exec(World, TEXT("quit"));
            }
        }
    }
}

// ---- Phase A helpers ----

ADroppedItemActor* UDevModeBotBrain::FindNearestDroppedItem(float MaxDistance) const
{
    UWorld* World = BrainWorld.Get();
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!World || !Player) { return nullptr; }

    ADroppedItemActor* Best = nullptr;
    float BestDist = MaxDistance;
    const FVector At = Player->GetActorLocation();
    for (TActorIterator<ADroppedItemActor> It(World); It; ++It)
    {
        ADroppedItemActor* Drop = *It;
        if (!IsValid(Drop)) { continue; }
        const float D = Dist2D(At, Drop->GetActorLocation());
        if (D < BestDist) { BestDist = D; Best = Drop; }
    }
    return Best;
}

ABasicMOB* UDevModeBotBrain::ResolveCorpseActor() const
{
    // Our victim if its corpse actor still stands (usual case)...
    if (TargetMob.IsValid() && !TargetMob->IsActorBeingDestroyed())
    {
        return TargetMob.Get();
    }
    // ...else the nearest corpse — the bot stands on its kill, so the
    // nearest one is ours unless the server already cleaned it up.
    if (GameInstance && GameInstance->GetHarvestManager())
    {
        UHarvestManager* Harvest = GameInstance->GetHarvestManager();
        if (ABasicMOB* Harvestable = Harvest->GetNearestHarvestableCorpse(600.0f))
        {
            return Harvestable;
        }
        return Harvest->GetNearestHarvestedCorpse(600.0f);
    }
    return nullptr;
}

bool UDevModeBotBrain::SpawnStubDrop(ABasicPlayer* Player)
{
    if (!GameInstance || !Player) { return false; }
    UItemManager* ItemManager = GameInstance->GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT stub drop failed: no ItemManager"));
        return false;
    }
    if (GameInstance->GetDroppedItemActorClass() == nullptr)
    {
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT stub drop failed: no DroppedItemActorClass"));
        return false;
    }

    // Dev-range UID: never collides with seeded ids (101..301) or mob UIDs.
    FDroppedItemStruct Drop;
    Drop.uid = LootDropUID;
    Drop.itemId = LootItemTemplateId;
    Drop.item.id = LootItemTemplateId;
    Drop.item.name = TEXT("Health Potion");
    Drop.item.slug = TEXT("health_potion");
    Drop.quantity = 1;
    Drop.canBePickedUp = true;
    const FVector At = LastKillLocation;
    Drop.position.positionX = At.X;
    Drop.position.positionY = At.Y;
    Drop.position.positionZ = At.Z + 50.0;

    ADroppedItemActor* Actor = ItemManager->SpawnDroppedItem(Drop);
    if (!IsValid(Actor))
    {
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT stub drop failed: SpawnDroppedItem returned null"));
        return false;
    }
    LootDropActor = Actor;
    UE_LOG(LogGameplay, Warning, TEXT("%s_LOOT_STUB uid=%d 'Health Potion' at corpse (offline drop, real spawn path)"),
        *TombstonePrefix, LootDropUID);
    return true;
}

void UDevModeBotBrain::ResolveStubLoot(ABasicPlayer* Player)
{
    if (!GameInstance || !Player) { return; }
    // Offline: no server confirm exists, so the pickup resolves through the
    // same local calls the server path ends in (inventory add + pickup
    // broadcast + world actor removal). Walking here and the asserts are
    // real; only the server round-trip is stubbed.
    FInventoryItemStruct NewItem;
    NewItem.id = LootDropUID;
    NewItem.itemId = LootItemTemplateId;
    NewItem.quantity = 1;
    NewItem.name = TEXT("Health Potion");
    NewItem.slug = TEXT("health_potion");
    NewItem.itemSlug = TEXT("health_potion");
    if (UInventoryManager* Inv = GameInstance->GetInventoryManager())
    {
        Inv->AddItemToLocalInventory(NewItem);
    }
    FItemBaseStruct Base;
    Base.id = LootItemTemplateId;
    Base.name = TEXT("Health Potion");
    Base.slug = TEXT("health_potion");
    if (UItemManager* Items = GameInstance->GetItemManager())
    {
        Items->ProcessItemPickup(Base);
    }
    if (ADroppedItemActor* Drop = LootDropActor.Get())
    {
        Drop->PlayPickupEffect();
        Drop->Destroy();
    }
    LootDropActor.Reset();
    bPickupRequested = true;
    UE_LOG(LogGameplay, Warning, TEXT("%s_LOOT_STUB resolved uid=%d into inventory (offline pickup)"),
        *TombstonePrefix, LootDropUID);
}

void UDevModeBotBrain::ApplyStubDeath(ABasicPlayer* Player)
{
    if (!Player) { return; }
    // Offline: no server damage exists. The death goes through the real
    // SetDead(true) path (death anim, movement lock, death screen) — the
    // same call server-driven death takes. Only the damage is stubbed.
    Player->SetPlayerCurrentHPPoints(0);
    Player->SetDead_Implementation(true);
    UE_LOG(LogGameplay, Warning, TEXT("%s_DEATH_STUB player killed offline (real SetDead path)"),
        *TombstonePrefix);
}

void UDevModeBotBrain::ApplyStubRevive(ABasicPlayer* Player)
{
    if (!Player) { return; }
    // Offline: no server respawnResult exists. Revive through the real
    // SetDead(false) path (movement restore, screen hide, OnRevive) at the
    // run spawn with full HP — the same observable state a server respawn
    // produces. Only the packet is stubbed.
    const int32 MaxHP = Player->GetMaxHealth_Implementation();
    Player->SetActorLocation(RespawnPoint);
    Player->SetDead_Implementation(false);
    Player->SetPlayerCurrentHPPoints(MaxHP > 0 ? MaxHP : 1000);
    UE_LOG(LogGameplay, Warning, TEXT("%s_RESPAWN_STUB player revived at spawn hp=%d (real SetDead path)"),
        *TombstonePrefix, Player->GetPlayerCurrentHPPoints());
}

int32 UDevModeBotBrain::CountInventoryItems() const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        return GameInstance->GetInventoryManager()->GetInventory().items.Num();
    }
    return -1;
}

int32 UDevModeBotBrain::SumInventoryQuantities() const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        int32 Total = 0;
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            Total += Item.quantity;
        }
        return Total;
    }
    return -1;
}

bool UDevModeBotBrain::IsDeathScreenVisible() const
{
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    UUIManager* UI = Player ? Player->GetUIManager() : nullptr;
    return UI ? UI->IsDeathScreenVisible() : false;
}

// ---- Phase B helpers ----

int32 UDevModeBotBrain::GetGold() const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        return GameInstance->GetInventoryManager()->GetInventory().gold;
    }
    return -1;
}

int32 UDevModeBotBrain::FindSellableInstanceId(int32& OutQty) const
{
    OutQty = 0;
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        // Junk = stackable, non-equipped, non-durable, non-quest, non-currency
        // row. The row survives the sale (qty - 1), so the assert reads qty
        // exactly. Currency rows (gold_coin, qty == balance) must never be
        // sold — the server eats the coin without credit (seen live).
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.quantity > 1 && !Item.is_equipped && !Item.isDurable && !Item.isQuestItem
                && !Item.itemTypeSlug.Equals(TEXT("currency"), ESearchCase::IgnoreCase))
            {
                OutQty = Item.quantity;
                return Item.id;
            }
        }
    }
    return 0;
}

int32 UDevModeBotBrain::FindDamagedInstanceId(int32& OutMaxDur) const
{
    OutMaxDur = 0;
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.durabilityMax > 0 && Item.durabilityCurrent < Item.durabilityMax)
            {
                OutMaxDur = Item.durabilityMax;
                return Item.id;
            }
        }
    }
    return 0;
}

int32 UDevModeBotBrain::GetQtyByInstance(int32 InstanceId) const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.id == InstanceId)
            {
                return Item.quantity;
            }
        }
    }
    return -1;
}

int32 UDevModeBotBrain::GetDurabilityByInstance(int32 InstanceId) const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.id == InstanceId)
            {
                return Item.durabilityCurrent;
            }
        }
    }
    return -1;
}

// ---- Phase C helpers ----

bool UDevModeBotBrain::HasItemTemplate(int32 TemplateItemId) const
{
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            if (Item.itemId == TemplateItemId && Item.quantity > 0)
            {
                return true;
            }
        }
    }
    return false;
}

ABasicPlayer* UDevModeBotBrain::FindPeerActor() const
{
    if (!GameInstance)
    {
        return nullptr;
    }
    // This client owns exactly one entry: any other live actor is the peer.
    const int32 OwnId = GameInstance->GetCurrentClientID();
    for (const TPair<int32, ABasicPlayer*>& E : GameInstance->SpawnedPlayers)
    {
        if (E.Key != OwnId && IsValid(E.Value) && !E.Value->IsActorBeingDestroyed())
        {
            return E.Value;
        }
    }
    return nullptr;
}

// ---- Phase D helpers ----

bool UDevModeBotBrain::HasActiveEffect(const FString& Slug) const
{
    if (GameInstance && GameInstance->GetPlayerStatsManager())
    {
        for (const FActiveEffectEntry& E : GameInstance->GetPlayerStatsManager()->GetCachedStats().activeEffects)
        {
            if (E.slug.Equals(Slug, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
    }
    return false;
}

int32 UDevModeBotBrain::FindEquippableMainHand(FString& OutSlug) const
{
    OutSlug.Empty();
    if (GameInstance && GameInstance->GetInventoryManager())
    {
        for (const FInventoryItemStruct& Item : GameInstance->GetInventoryManager()->GetInventory().items)
        {
            // Inventory rows carry the slug in `slug` (`itemSlug` belongs to
            // other structs and stays empty here) — mirror the ItemTooltip
            // fallback instead of trusting one field.
            const FString& Slug = Item.slug.IsEmpty() ? Item.itemSlug : Item.slug;
            if (Item.isEquippable && !Item.is_equipped
                && Item.equipSlotSlug.Equals(TEXT("main_hand"), ESearchCase::IgnoreCase))
            {
                OutSlug = Slug;
                return Item.id;
            }
        }
    }
    return 0;
}

int32 UDevModeBotBrain::GetProgressionLevel() const
{
    if (GameInstance && GameInstance->GetExperienceManager())
    {
        return GameInstance->GetExperienceManager()
            ->GetCharacterProgression(GameInstance->GetCurrentCharacterID()).currentLevel;
    }
    return 0;
}

int32 UDevModeBotBrain::GetProgressionExp() const
{
    if (GameInstance && GameInstance->GetExperienceManager())
    {
        return GameInstance->GetExperienceManager()
            ->GetCharacterProgression(GameInstance->GetCurrentCharacterID()).totalExperience;
    }
    return 0;
}

// ---- Phase E handlers ----

