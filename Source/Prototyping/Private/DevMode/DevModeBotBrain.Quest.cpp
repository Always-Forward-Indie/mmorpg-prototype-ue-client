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
#include "Utils/NetCore/QuestEdgeRule.h"

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
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT dialogue opened npc=%d"), NpcId);
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
    // Predicate lives in FQuestEdgeRule (pure, spec-pinned); here only the
    // 1:1 field mapping from the live dialogue node remains.
    TArray<FQuestEdgeRule::FChoice> Edges;
    for (const FDialogueChoice& Choice : Dlg->GetCurrentNode().choices)
    {
        FQuestEdgeRule::FChoice E;
        E.EdgeId = Choice.edgeId;
        E.bHasQuestPreview = Choice.bHasQuestPreview;
        E.bHasTurnInPreview = Choice.bHasTurnInPreview;
        E.bConditionMet = Choice.conditionMet;
        E.ChoiceKey = Choice.clientChoiceKey;
        Edges.Add(E);
    }
    return FQuestEdgeRule::FindGoalEdge(Edges, bWantTurnin);
}

int32 UDevModeBotBrain::FindAdvanceEdge() const
{
    UDialogueManager* Dlg = GameInstance ? GameInstance->GetDialogueManager() : nullptr;
    if (!Dlg)
    {
        return 0;
    }
    // Predicate lives in FQuestEdgeRule (pure, spec-pinned); here only the
    // 1:1 field mapping from the live dialogue node remains.
    TArray<FQuestEdgeRule::FChoice> Edges;
    for (const FDialogueChoice& Choice : Dlg->GetCurrentNode().choices)
    {
        FQuestEdgeRule::FChoice E;
        E.EdgeId = Choice.edgeId;
        E.bHasQuestPreview = Choice.bHasQuestPreview;
        E.bHasTurnInPreview = Choice.bHasTurnInPreview;
        E.bConditionMet = Choice.conditionMet;
        E.ChoiceKey = Choice.clientChoiceKey;
        Edges.Add(E);
    }
    return FQuestEdgeRule::FindAdvanceEdge(Edges);
}

void UDevModeBotBrain::QuestLootDone(double Now)
{
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    // Live: the turn-in needs 2 kills AND a harvest-only hide (45% per
    // corpse). Extra kills are harmless (kill step is already done), so
    // keep hunting until the hide lands — capped to fail loud, not loop.
    // Offline loot is stubbed (no hide concept): keep the old gate.
    if (Kills >= TargetKills && !bStubLoot && !HasItemTemplate(57))
    {
        if (Kills < 6)
        {
            ++TargetKills;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT no hide yet after %d kills, hunting on (need 2 + hide)"), Kills);
        }
        else
        {
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT no hide in 6 kills, turning in anyway"));
        }
    }
    if (Kills >= TargetKills && (bStubLoot || HasItemTemplate(57) || Kills >= 6))
    {
        Step = EBotStep::QuestTurnin;
        QuestLegStartTime = Now;
        bDialogueRequested = false;
        bChoiceSent = false;
        DialogueClicks = 0;
        LastAdvanceNodeId = 0;
        GoldBeforeReward = GetGold();
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT objectives done, returning to turn in"));
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
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT corpse resolved, hunting next (%d/%d)"), Kills, TargetKills);
}
