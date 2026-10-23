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
#include "Utils/NetCore/ThrowawayNameRule.h"

bool UDevModeBotBrain::Tick_CharList(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (!Auth)
        {
            Finish(false, TEXT("no-auth"));
            return true;
        }
        if (!bListReqSent)
        {
            Auth->OnCharacterListReceived.AddDynamic(this, &UDevModeBotBrain::HandleCharListReceived);
            Auth->OnCreateCharacterResponse.AddDynamic(this, &UDevModeBotBrain::HandleCreateResponse);
            Auth->OnDeleteCharacterResponse.AddDynamic(this, &UDevModeBotBrain::HandleDeleteResponse);
            FClientDataStruct ClientData = GameInstance->GetCurrentClientData();
            Auth->SendCharacterListRequest(ClientData);
            bListReqSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT character list requested"));
            return true;
        }
        if (CharListCache.Num() > 0)
        {
            const int32 OwnId = GameInstance->GetCurrentCharacterID();
            bool bFound = false;
            for (const FLoginCharacterEntry& E : CharListCache)
            {
                if (E.CharacterId == OwnId) { bFound = true; break; }
            }
            BotAssert(bFound, TEXT("char-list-self"), bFound ? 1 : 0, 1);
            Step = EBotStep::CharCreate;
            FlowLegStartTime = Now;
            return true;
        }
        if (Now - FlowLegStartTime > FlowTimeoutSec)
        {
            Finish(false, TEXT("char-list-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_CharCreate(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (!Auth)
        {
            Finish(false, TEXT("no-auth"));
            return true;
        }
        if (!CreateError.IsEmpty())
        {
            BotAssert(false, TEXT("char-created"), 0, 1);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT create failed: %s"), *CreateError);
            Finish(false, TEXT("create-failed"));
            return true;
        }
        if (NewCharId > 0)
        {
            // Verify the newcomer shows up in a fresh list.
            if (!bListReqSent)
            {
                FClientDataStruct ClientData = GameInstance->GetCurrentClientData();
                Auth->SendCharacterListRequest(ClientData);
                bListReqSent = true;
                return true;
            }
            for (const FLoginCharacterEntry& E : CharListCache)
            {
                if (E.CharacterId == NewCharId)
                {
                    BotAssert(true, TEXT("char-created"), NewCharId, NewCharId);
                    BotShot(TEXT("created"));
                    Step = EBotStep::CharDelete;
                    FlowLegStartTime = Now;
                    return true;
                }
            }
            if (Now - FlowLegStartTime > FlowTimeoutSec)
            {
                BotAssert(false, TEXT("char-created"), 0, NewCharId);
                Finish(false, TEXT("create-verify-timeout"));
            }
            return true;
        }
        if (CharName.IsEmpty())
        {
            // Slice 2 (variant B): timestamped unique Qabot names, no
            // ID-parsing races. Letters only (server rejects digits/
            // underscores), seed = wall time mixed with rand. The old
            // Test V..Z cycle exhausted after 5 runs (names-exhausted).
            if (NameLetterIdx > 10)
            {
                Finish(false, TEXT("names-exhausted"));
                return true;
            }
            const uint32 Seed = static_cast<uint32>(FDateTime::Now().ToUnixTimestamp() & 0xffffffff)
                ^ static_cast<uint32>(FMath::Rand() << (NameLetterIdx % 16));
            CharName = FThrowawayNameRule::MakeQabotName(Seed);
        }
        if (!bCreateSent)
        {
            Auth->SendCreateCharacterRequest(CharName, TEXT("warrior"), TEXT("human"), TEXT("male"));
            bCreateSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT create requested '%s'"), *CharName);
        }
        if (Now - FlowLegStartTime > FlowTimeoutSec)
        {
            Finish(false, TEXT("create-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_CharDelete(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (!Auth)
        {
            Finish(false, TEXT("no-auth"));
            return true;
        }
        if (!DeleteError.IsEmpty())
        {
            BotAssert(false, TEXT("char-deleted"), 0, 1);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT delete failed: %s"), *DeleteError);
            Finish(false, TEXT("delete-failed"));
            return true;
        }
        if (!bDeleteSent)
        {
            Auth->SendDeleteCharacterRequest(NewCharId);
            bDeleteSent = true;
            bListReqSent = false;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT delete requested id=%d"), NewCharId);
            return true;
        }
        if (!bListReqSent)
        {
            FClientDataStruct ClientData = GameInstance->GetCurrentClientData();
            Auth->SendCharacterListRequest(ClientData);
            bListReqSent = true;
            return true;
        }
        for (const FLoginCharacterEntry& E : CharListCache)
        {
            if (E.CharacterId == NewCharId)
            {
                if (Now - FlowLegStartTime > FlowTimeoutSec)
                {
                    BotAssert(false, TEXT("char-deleted"), 1, 0);
                    Finish(false, TEXT("delete-timeout"));
                }
                return true;
            }
        }
        BotAssert(true, TEXT("char-deleted"), 1, 1);
        BotShot(TEXT("deleted"));
        Finish(Failures == 0, Failures == 0 ? TEXT("char-done") : TEXT("assert-failures"));
        return true;
}

bool UDevModeBotBrain::Tick_BackToLogin(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // World timers die in the travel below: arm the PostLoadMap hook
        // and a core-ticker watchdog, then travel. The tombstone fires in
        // the callback, never in BrainTick.
        if (!PostLoadMapLoginHandle.IsValid())
        {
            PostLoadMapLoginHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
                this, &UDevModeBotBrain::HandlePostLoadMapLogin);
        }
        if (!bLoginTravelSent)
        {
            GameInstance->ReturnToLoginLevel();
            bLoginTravelSent = true;
            UE_LOG(LogTemp, Warning, TEXT("%s returning to login"), *TombstonePrefix);
            TWeakObjectPtr<UDevModeBotBrain> WeakThis(this);
            const double WatchStart = FPlatformTime::Seconds();
            FTSTicker::GetCoreTicker().RemoveTicker(LoginWatchdogHandle);
            LoginWatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([WeakThis, WatchStart](float)
                {
                    UDevModeBotBrain* Self = WeakThis.Get();
                    if (!Self || !Self->IsRunning()) { return false; }
                    if (FPlatformTime::Seconds() - WatchStart > 120.0)
                    {
                        Self->Finish(false, TEXT("login-travel-timeout"));
                    }
                    return Self->IsRunning();
                }),
                1.0f);
        }
        return true;
}
