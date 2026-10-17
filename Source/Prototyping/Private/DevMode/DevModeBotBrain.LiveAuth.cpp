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

void UDevModeBotBrain::HandleCharListReceived(const TArray<FLoginCharacterEntry>& Characters)
{
    CharListCache = Characters;
    UE_LOG(LogTemp, Log, TEXT("DEVMODE_BOT char list: %d entries"), Characters.Num());
}

void UDevModeBotBrain::HandleCreateResponse(bool bSuccess, const FString& Message, int32 CharacterId)
{
    if (bSuccess)
    {
        NewCharId = CharacterId;
        bListReqSent = false;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT created id=%d"), CharacterId);
        return;
    }
    // Name taken (a previous run's leftover): rotate to the next letter.
    if (Message.Contains(TEXT("taken"), ESearchCase::IgnoreCase)
        || Message.Contains(TEXT("exists"), ESearchCase::IgnoreCase))
    {
        ++NameLetterIdx;
        CharName.Empty();
        bCreateSent = false;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT name taken, rotating (%s)"), *Message);
        return;
    }
    CreateError = Message;
}

void UDevModeBotBrain::HandleDeleteResponse(bool bSuccess, const FString& Message, int32 CharacterId)
{
    if (!bSuccess)
    {
        DeleteError = Message;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT delete failed id=%d: %s"), CharacterId, *Message);
    }
}

void UDevModeBotBrain::HandlePostLoadMapLogin(UWorld* LoadedWorld)
{
    if (!bRunning)
    {
        return;
    }
    const FString MapName = LoadedWorld ? LoadedWorld->GetMapName() : TEXT("null");
    UE_LOG(LogTemp, Warning, TEXT("%s post-travel map: %s"), *TombstonePrefix, *MapName);
    if (MapName.Contains(TEXT("Login")))
    {
        BotAssert(true, TEXT("back-to-login"), 1, 1);
        BotShot(TEXT("login"));
        Finish(true, TEXT("back-to-login-done"));
        // Finish quits via the (here torn-down) run world: quit explicitly
        // from the freshly loaded login world instead.
        if (LoadedWorld && GEngine)
        {
            GEngine->Exec(LoadedWorld, TEXT("quit"));
        }
        return;
    }
    // Some other map (boot/world again): keep waiting for the login one;
    // the core-ticker watchdog bounds the wait.
}

