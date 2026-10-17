#include "DevMode/DevModeBotBrain.h"
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

bool UDevModeBotBrain::EnsureDialogueOpen(int32 NpcId, double Now)
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return false;
    }
    if (Dlg->IsDialogueActive())
    {
        return true;
    }
    // (Re)open, throttled: covers the first open and server-side closes
    // (TTL, end-nodes) mid-leg.
    if (!bDialogueRequested || Now - LastDialogueRequestTime > 5.0)
    {
        Dlg->OpenDialogue(NpcId);
        bDialogueRequested = true;
        LastDialogueRequestTime = Now;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT dialogue opened npc=%d"), NpcId);
    }
    return false;
}

int32 UDevModeBotBrain::FindGoalEdge(bool bWantTurnin) const
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return 0;
    }
    for (const FDialogueChoice& Choice : Dlg->GetCurrentNode().choices)
    {
        const bool bGoal = bWantTurnin ? Choice.bHasTurnInPreview : Choice.bHasQuestPreview;
        if (bGoal && Choice.conditionMet)
        {
            return Choice.edgeId;
        }
    }
    return 0;
}

int32 UDevModeBotBrain::FindAdvanceEdge() const
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return 0;
    }
    // Neutral hubs (continue/back): first met condition that is NOT a
    // farewell-class edge. Goal edges have priority in the caller, so this
    // never steals an accept/turn-in.
    for (const FDialogueChoice& Choice : Dlg->GetCurrentNode().choices)
    {
        if (!Choice.conditionMet)
        {
            continue;
        }
        const FString Key = Choice.clientChoiceKey.ToLower();
        if (Key.Contains(TEXT("farewell")) || Key.Contains(TEXT("bye")) || Key.Contains(TEXT("leave"))
            || Key.Contains(TEXT("goodbye")) || Key.Contains(TEXT("close")) || Key.Contains(TEXT("exit"))
            || Key.Contains(TEXT("cancel")))
        {
            continue;
        }
        return Choice.edgeId;
    }
    return 0;
}

void UDevModeBotBrain::QuestLootDone(double Now)
{
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (Kills >= TargetKills)
    {
        Step = EBotStep::QuestTurnin;
        QuestLegStartTime = Now;
        bDialogueRequested = false;
        bChoiceSent = false;
        DialogueClicks = 0;
        LastAdvanceNodeId = 0;
        GoldBeforeReward = GetGold();
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT objectives done, returning to turn in"));
        return;
    }
    // More kills to go: this corpse is resolved (looted or empty), hunt on.
    TargetMob.Reset();
    bHarvestRequested = false;
    bInspectRequested = false;
    bPickupRequested = false;
    CorpseUID = 0;
    Step = EBotStep::GotoTarget;
    StepStartTime = Now;
    if (Player)
    {
        LastChasePos = Player->GetActorLocation();
        LastChaseProgressTime = Now;
    }
    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT corpse resolved, hunting next (%d/%d)"), Kills, TargetKills);
}
