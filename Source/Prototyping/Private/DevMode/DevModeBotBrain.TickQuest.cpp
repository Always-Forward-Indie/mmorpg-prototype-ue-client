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

bool UDevModeBotBrain::Tick_QuestWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Honest walk to the quest giver (dialogue range is server-side).
        if (SteerToward(Player, QuestNpcPos, 150.0f, Now, TEXT("quest-npc")))
        {
            Step = EBotStep::QuestAccept;
            QuestLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at quest npc=%d, opening dialogue"), QuestNpcId);
            return true;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            Finish(false, TEXT("quest-unreachable"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_QuestAccept(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UDialogueManager* Dlg = GameInstance->GetDialogueManager();
        UQuestManager* Qst = GameInstance->GetQuestManager();
        if (!Dlg || !Qst)
        {
            Finish(false, TEXT("no-dialogue"));
            return true;
        }
        // Accepted already (choice landed, or a rerun mid-chain): hunt.
        if (Qst->HasQuest(QuestId))
        {
            BotAssert(true, TEXT("quest-accepted"), QuestId, QuestId);
            BotShot(TEXT("accepted"));
            if (Dlg->IsDialogueActive())
            {
                Dlg->CloseDialogue();
            }
            // Objectives run on the shared combat/loot machinery (2 real
            // kills + harvest + loot); the server credits them to the quest.
            TargetMob.Reset();
            Step = EBotStep::GotoTarget;
            StepStartTime = Now;
            LastChasePos = PlayerAt;
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT quest accepted, hunting objectives"));
            return true;
        }
        if (!EnsureDialogueOpen(QuestNpcId, Now))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("accept-timeout"));
            }
            return true;
        }
        if (const int32 AcceptEdge = FindGoalEdge(false))
        {
            if (!bChoiceSent)
            {
                Dlg->SendChoice(AcceptEdge);
                bChoiceSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT accept choice sent edge=%d"), AcceptEdge);
            }
            // Quest grant lands as a journal push; polled at the top on the
            // next ticks (HasQuest branch above).
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("accept-timeout"));
            }
            return true;
        }
        // No offer on this node: walk the dialogue (greeting -> hub).
        if (DialogueClicks >= 10)
        {
            Finish(false, TEXT("dialogue-loop"));
            return true;
        }
        const int32 NodeId = Dlg->GetCurrentNode().nodeId;
        if (const int32 AdvanceEdge = FindAdvanceEdge())
        {
            if (NodeId != LastAdvanceNodeId)
            {
                Dlg->SendChoice(AdvanceEdge);
                LastAdvanceNodeId = NodeId;
                ++DialogueClicks;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT dialogue advance edge=%d (node %d)"),
                    AdvanceEdge, NodeId);
            }
            return true;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT no accept/advance edge in node %d (%d choices)"),
                NodeId, Dlg->GetCurrentNode().choices.Num());
            Finish(false, TEXT("no-accept-choice"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_QuestTurnin(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UDialogueManager* Dlg = GameInstance->GetDialogueManager();
        UQuestManager* Qst = GameInstance->GetQuestManager();
        if (!Dlg || !Qst)
        {
            Finish(false, TEXT("no-dialogue"));
            return true;
        }
        // Walk back first (objectives may have pulled us far away). The
        // server gates dialogue on proximity: only talk once arrived.
        if (!SteerToward(Player, QuestNpcPos, 150.0f, Now, TEXT("turnin-npc")))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("turnin-unreachable"));
            }
            return true;
        }
        if (!EnsureDialogueOpen(QuestNpcId, Now))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("turnin-timeout"));
            }
            return true;
        }
        if (const int32 TurninEdge = FindGoalEdge(true))
        {
            if (!bChoiceSent)
            {
                Dlg->SendChoice(TurninEdge);
                bChoiceSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turnin choice sent edge=%d"), TurninEdge);
                return true;
            }
        }
        // Reward ground truth (checked regardless of the current node: the
        // turn-in choice advances past the turn-in edge): +5g floor (loot
        // gold may add more) and the freshly granted potion.
        if (bChoiceSent && GetGold() >= GoldBeforeReward + 5 && HasItemTemplate(46))
        {
            const FQuestProgressData Q = Qst->GetQuestData(QuestId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turned in: gold %d -> %d queststate='%s'"),
                GoldBeforeReward, GetGold(), *Q.state);
            BotAssert(true, TEXT("quest-reward-gold"), 1, 1);
            BotAssert(true, TEXT("quest-reward-item"), 46, 46);
            BotShot(TEXT("turnedin"));
            Dlg->CloseDialogue();
            Finish(Failures == 0, Failures == 0 ? TEXT("quest-done") : TEXT("assert-failures"));
            return true;
        }
        if (bChoiceSent)
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                BotAssert(false, TEXT("quest-reward-gold"), GetGold(), GoldBeforeReward + 5);
                Finish(false, TEXT("turnin-timeout"));
            }
            return true;
        }
        // Objectives not yet credited server-side, or a hub without the
        // turn-in: advance neutral hubs, budget the silence.
        if (DialogueClicks >= 10)
        {
            Finish(false, TEXT("dialogue-loop"));
            return true;
        }
        const int32 NodeId = Dlg->GetCurrentNode().nodeId;
        if (const int32 AdvanceEdge = FindAdvanceEdge())
        {
            if (NodeId != LastAdvanceNodeId)
            {
                Dlg->SendChoice(AdvanceEdge);
                LastAdvanceNodeId = NodeId;
                ++DialogueClicks;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turnin dialogue advance edge=%d (node %d)"),
                    AdvanceEdge, NodeId);
            }
            return true;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            const FQuestProgressData Q = Qst->GetQuestData(QuestId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT no turnin edge in node %d (%d choices) queststate='%s' step='%s' %d/%d"),
                NodeId, Dlg->GetCurrentNode().choices.Num(), *Q.state, *Q.currentStepKey, Q.stepCurrentCount, Q.stepRequiredCount);
            Finish(false, TEXT("no-turnin-choice"));
        }
        return true;
}
