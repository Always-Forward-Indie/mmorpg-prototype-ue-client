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
#include "Gameplay/Trade/TradeManager.h"
#include "Utils/NetCore/TradeStatusRule.h"
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
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT anchored at rendezvous, settling before speaking"));
                return true;
            }
            if (Now - PeerCloseSince < 3.0)
            {
                return true;
            }
            // One short "local" message each; both sides send, both assert
            // the peer's arrival (own echo, if any, is filtered by sender).
            // Re-sent while waiting: a message spoken before the peer's
            // client finished loading is lost to them (no chat history), and
            // the early speaker must still be talking when the late joiner
            // starts listening. Finishing waits for the leg budget, never
            // for the first receipt — otherwise the early side quits before
            // the late side heard anything.
            Chat->SendChatMessage(TEXT("local"), FString::Printf(TEXT("hi-%d"), OwnCharId));
            bChatSent = true;
            ChatLegStartTime = Now;
            LastChatResendTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT chat sent hi-%d from (%.0f,%.0f,%.0f)"),
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
        if (!PeerChatText.IsEmpty() && !bPeerMsgSeen)
        {
            bPeerMsgSeen = true;
            BotAssert(true, TEXT("peer-msg-received"), 1, 1);
            // The bubble renders on the peer actor's nameplate through the
            // same ShowChatBubble call live players trigger. Checked at
            // receipt: bubbles expire, a timeout-time check would lie.
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
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT peer message received: %s"), *PeerChatText);
        }
        else if (PeerChatText.IsEmpty() && Now - LastChatResendTime > 10.0)
        {
            Chat->SendChatMessage(TEXT("local"), FString::Printf(TEXT("hi-%d"), OwnCharId));
            LastChatResendTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT chat re-sent hi-%d (peer msg not yet seen)"), OwnCharId);
        }
        if (Now - ChatLegStartTime > ChatTimeoutSec)
        {
            if (!bPeerMsgSeen)
            {
                BotAssert(false, TEXT("peer-msg-received"), 0, 1);
                Finish(false, TEXT("chat-timeout"));
            }
            else
            {
                Finish(Failures == 0, Failures == 0 ? TEXT("chat-done") : TEXT("assert-failures"));
            }
        }
        return true;
}

bool UDevModeBotBrain::Tick_TradeElect(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UTradeManager* Trade = GameInstance ? GameInstance->GetTradeManager() : nullptr;
        if (!Trade)
        {
            Finish(false, TEXT("no-trade"));
            return true;
        }
        ABasicPlayer* Peer = FindPeerActor();
        if (!IsValid(Peer))
        {
            Finish(false, TEXT("peer-lost"));
            return true;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        TradePeerCharId = Peer->GetPlayerCharacterID();
        if (TradePeerCharId <= 0)
        {
            // Remote actor visible but its character id not replicated yet —
            // wait for it to settle instead of electing on garbage.
            if (Now - TradeLegStartTime > TradeTimeoutSec)
            {
                Finish(false, TEXT("peer-id-timeout"));
            }
            return true;
        }
        // Election lives in FTradeStatusRule (spec-pinned): lower id invites.
        // NOTE: GoldBeforeTrade is captured at offer time, not here (a grant landing in between would poison it).
        // Baseline needs the authoritative inventory snapshot (async push;
        // fat snapshots fragment — proven live 22.10). Capturing GetGold()
        // on an empty cache would poison the gold-moved assert with 0, so
        // wait for the snapshot inside the leg budget instead.
        if (CountInventoryItems() <= 0)
        {
            if (UInventoryManager* Inv = GameInstance->GetInventoryManager())
            {
                if (Now - LastInvResyncTime > 15.0)
                {
                    LastInvResyncTime = Now;
                    Inv->RequestInventoryData(GameInstance->GetCurrentCharacterID());
                    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade waits for inventory snapshot"));
                }
            }
            if (Now - TradeLegStartTime > TradeTimeoutSec)
            {
                Finish(false, TEXT("trade-no-inventory"));
            }
            return true;
        }
        BotAssert(true, TEXT("trade-peer-found"), TradePeerCharId, TradePeerCharId);
        if (FTradeStatusRule::ShouldInitiateTrade(OwnCharId, TradePeerCharId))
        {
            Step = EBotStep::TradeInvite;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade: inviting char %d (we are %d)"),
                TradePeerCharId, OwnCharId);
        }
        else
        {
            Step = EBotStep::TradeAccept;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade: accepting from char %d (we are %d)"),
                TradePeerCharId, OwnCharId);
        }
        TradeLegStartTime = Now;
        return true;
}

bool UDevModeBotBrain::Tick_TradeInvite(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UTradeManager* Trade = GameInstance ? GameInstance->GetTradeManager() : nullptr;
        if (!Trade)
        {
            Finish(false, TEXT("no-trade"));
            return true;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        if (!bTradeInviteSent)
        {
            Trade->RequestTrade(OwnCharId, TradePeerCharId, Player->GetActorLocation());
            bTradeInviteSent = true;
            LastInviteResendTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade invite sent to char %d"), TradePeerCharId);
            return true;
        }
        // Session id arrives on the tradeState echo — that is the invite proof.
        // No session yet: the invite is one-shot but the server TTL is 60s
        // while the peer starts ~35s later — re-send to refresh TTL and
        // cover a lost first send (same pattern as the chat resend).
        if (bTradeInviteSent && Now - LastInviteResendTime > 20.0)
        {
            LastInviteResendTime = Now;
            Trade->RequestTrade(OwnCharId, TradePeerCharId, Player->GetActorLocation());
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade invite re-sent to char %d"), TradePeerCharId);
        }
        if (Trade->IsInTrade())
        {
            BotAssert(true, TEXT("trade-invited"), 1, 1);
            BotShot(TEXT("trade-invite"));
            // Authoritative baseline: the session echo carries live balances
            // (immune to stale/dropped inventory snapshots).
            if (!bTradeGoldBaselined)
            {
                GoldBeforeTrade = Trade->GetCurrentState().myGoldBalance;
                bTradeGoldBaselined = true;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade baseline gold=%d (session echo)"), GoldBeforeTrade);
            }
            Step = EBotStep::TradeOffer;
            TradeLegStartTime = Now;
            return true;
        }
        if (Now - TradeLegStartTime > TradeTimeoutSec)
        {
            Finish(false, TEXT("trade-invite-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_TradeAccept(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UTradeManager* Trade = GameInstance ? GameInstance->GetTradeManager() : nullptr;
        if (!Trade)
        {
            Finish(false, TEXT("no-trade"));
            return true;
        }
        if (!bTradeBogusAcceptSent)
        {
            // S12 no_pending_invite probe: answer with OUR OWN id first.
            // The server matches invites on the initiator id, so this must
            // come back as tradeCancelled reason=no_pending_invite (surfaced
            // by TradeNetworkHandler as a cancel). Proven once; the normal
            // accept below then proceeds untouched.
            const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
            Trade->RespondToTradeInvite(OwnCharId, true);
            bTradeBogusAcceptSent = true;
            TradeBogusAcceptTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade bogus accept sent (expecting no_pending_invite cancel)"));
            return true;
        }
        if (!bTradeBogusAcceptProven)
        {
            const FString Reason = Trade->GetLastCancelData().reason;
            if (Reason.Contains(TEXT("no_pending_invite")))
            {
                bTradeBogusAcceptProven = true;
                BotAssert(true, TEXT("trade-bogus-cancelled"), 1, 1);
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT bogus accept correctly cancelled: %s"), *Reason);
            }
            else if (Now - TradeBogusAcceptTime > 15.0)
            {
                BotAssert(false, TEXT("trade-bogus-cancelled"), 0, 1);
                Finish(false, TEXT("bogus-accept-timeout"));
                return true;
            }
            else
            {
                return true;
            }
        }
        if (Trade->IsInTrade())
        {
            // Invite accepted (or the peer was faster) — session is live.
            BotAssert(true, TEXT("trade-accepted"), 1, 1);
            BotShot(TEXT("trade-accept"));
            if (!bTradeGoldBaselined)
            {
                GoldBeforeTrade = Trade->GetCurrentState().myGoldBalance;
                bTradeGoldBaselined = true;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade baseline gold=%d (session echo)"), GoldBeforeTrade);
            }
            Step = EBotStep::TradeOffer;
            TradeLegStartTime = Now;
            return true;
        }
        if (Trade->HasPendingInvite())
        {
            // Echo the INITIATOR's id (TradePeerCharId) — the server matches
            // the pending invite on it; our own id answers "no_pending_invite".
            Trade->RespondToTradeInvite(TradePeerCharId, true);
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade invite accepted"));
            return true;
        }
        if (Now - TradeLegStartTime > TradeTimeoutSec)
        {
            Finish(false, TEXT("trade-accept-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_TradeOffer(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UTradeManager* Trade = GameInstance ? GameInstance->GetTradeManager() : nullptr;
        if (!Trade)
        {
            Finish(false, TEXT("no-trade"));
            return true;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        if (!bTradeOfferSent)
        {
            // Baseline prefers the session echo (see Invite/Accept); only
            // fall back to the inventory snapshot when no echo seen yet
            // (snapshot can be stale post-rejoin or dropped oversized).
            if (!bTradeGoldBaselined)
            {
                if (Trade->IsInTrade())
                {
                    GoldBeforeTrade = Trade->GetCurrentState().myGoldBalance;
                    bTradeGoldBaselined = true;
                    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade baseline gold=%d (session echo, offer leg)"), GoldBeforeTrade);
                }
                else
                {
                    GoldBeforeTrade = GetGold();
                }
            }
            // Same-socket FIFO: the offer is processed before our confirm
            // below, so no echo wait is needed here. Initiator moves 1 gold,
            // acceptor offers nothing — the gold delta is the trade proof.
            const int32 OfferGold = FTradeStatusRule::ShouldInitiateTrade(OwnCharId, TradePeerCharId) ? 1 : 0;
            Trade->UpdateTradeOffer(OwnCharId, OfferGold, TArray<FTradeOfferItem>());
            bTradeOfferSent = true;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade offer sent: %d gold"), OfferGold);
            Step = EBotStep::TradeConfirm;
            TradeLegStartTime = Now;
            return true;
        }
        return true;
}

bool UDevModeBotBrain::Tick_TradeConfirm(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UTradeManager* Trade = GameInstance ? GameInstance->GetTradeManager() : nullptr;
        if (!Trade)
        {
            Finish(false, TEXT("no-trade"));
            return true;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        if (!bTradeConfirmSent)
        {
            Trade->ConfirmTrade(OwnCharId);
            bTradeConfirmSent = true;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade confirmed"));
            return true;
        }
        // Completion proof is the live balance: while the session is up the
        // authoritative session echo owns it (immune to stale/dropped
        // inventory snapshots — proven live 25.10); once the session is
        // wiped on tradeComplete, the inventory snapshot (fed by the resync
        // loop below) owns it.
        const bool bInitiator = FTradeStatusRule::ShouldInitiateTrade(OwnCharId, TradePeerCharId);
        const int32 GoldNow = Trade->IsInTrade() ? Trade->GetCurrentState().myGoldBalance : GetGold();
        const int32 GoldWant = bInitiator ? GoldBeforeTrade - 1 : GoldBeforeTrade + 1;
        if (GoldNow == GoldWant)
        {
            BotAssert(true, TEXT("trade-gold-moved"), GoldNow, GoldWant);
            BotShot(TEXT("trade"));
            Finish(Failures == 0, Failures == 0 ? TEXT("trade-done") : TEXT("assert-failures"));
            return true;
        }
        if (!Trade->IsInTrade())
        {
            // Session gone without our gold moving: usually cancelled —
            // BUT the session row is also wiped on a clean complete, and
            // our gold snapshot may still be in flight (lost push, proven
            // live 22.10 with 8KB+ snapshots). Re-request inventory
            // throttled and wait the leg budget before calling it.
            if (UInventoryManager* Inv = GameInstance->GetInventoryManager())
            {
                if (Now - LastInvResyncTime > 15.0)
                {
                    LastInvResyncTime = Now;
                    Inv->RequestInventoryData(GameInstance->GetCurrentCharacterID());
                    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT trade session gone, re-requesting inventory"));
                }
            }
            if (Now - TradeLegStartTime > TradeTimeoutSec)
            {
                BotAssert(false, TEXT("trade-gold-moved"), GoldNow, GoldWant);
                Finish(false, TEXT("trade-cancelled"));
            }
            return true;
        }
        if (Now - TradeLegStartTime > TradeTimeoutSec)
        {
            BotAssert(false, TEXT("trade-gold-moved"), GoldNow, GoldWant);
            Finish(false, TEXT("trade-confirm-timeout"));
        }
        return true;
}
