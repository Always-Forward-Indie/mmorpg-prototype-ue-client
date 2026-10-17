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

bool UDevModeBotBrain::Tick_ChatExchange(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UChatManager* Chat = GameInstance->GetChatManager();
        if (!Chat)
        {
            Finish(false, TEXT("no-chat"));
            return true;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        if (!bChatSent)
        {
            // Local chat only travels 50u server-side. Both sides anchor to
            // the SAME static rendezvous point (arrive tight) instead of
            // chasing the peer actor — replicated peer positions lag too
            // much to converge on. Then settle (server positions lag the
            // client tick) and speak.
            if (Dist2D(PlayerAt, RendezvousPoint) > 20.0f)
            {
                PeerCloseSince = 0.0;
                SteerToward(Player, RendezvousPoint, 20.0f, Now, TEXT("chat-anchor"));
                if (Now - ChatLegStartTime > ChatTimeoutSec)
                {
                    Finish(false, TEXT("peer-unreachable"));
                }
                return true;
            }
            if (!IsValid(FindPeerActor()))
            {
                Finish(false, TEXT("peer-lost"));
                return true;
            }
            if (PeerCloseSince <= 0.0)
            {
                PeerCloseSince = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT anchored at rendezvous, settling before speaking"));
                return true;
            }
            if (Now - PeerCloseSince < 3.0)
            {
                return true;
            }
            // One short "local" message each; both sides send, both assert
            // the peer's arrival (own echo, if any, is filtered by sender).
            Chat->SendChatMessage(TEXT("local"), FString::Printf(TEXT("hi-%d"), OwnCharId));
            bChatSent = true;
            ChatLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT chat sent hi-%d from (%.0f,%.0f,%.0f)"),
                OwnCharId, PlayerAt.X, PlayerAt.Y, PlayerAt.Z);
            return true;
        }
        // Peer message = hi-* from anyone but us.
        PeerChatText.Empty();
        for (const FChatMessageStruct& Msg : Chat->GetMessageLog())
        {
            if (!Msg.bIsError && Msg.senderId > 0 && Msg.senderId != OwnCharId && Msg.text.StartsWith(TEXT("hi-")))
            {
                PeerChatText = Msg.text;
                break;
            }
        }
        if (!PeerChatText.IsEmpty())
        {
            BotAssert(true, TEXT("peer-msg-received"), 1, 1);
            // The bubble renders on the peer actor's nameplate through the
            // same ShowChatBubble call live players trigger.
            bool bBubbleOk = false;
            if (ABasicPlayer* Peer = FindPeerActor())
            {
                if (UPlayerNameplateComponent* Plate = Peer->GetNameplateComponent())
                {
                    bBubbleOk = Plate->GetLastBubbleText() == PeerChatText;
                }
            }
            BotAssert(bBubbleOk, TEXT("peer-bubble"), bBubbleOk ? 1 : 0, 1);
            BotShot(TEXT("chat"));
            Finish(Failures == 0, Failures == 0 ? TEXT("chat-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - ChatLegStartTime > ChatTimeoutSec)
        {
            BotAssert(false, TEXT("peer-msg-received"), 0, 1);
            Finish(false, TEXT("chat-timeout"));
        }
        return true;
}
