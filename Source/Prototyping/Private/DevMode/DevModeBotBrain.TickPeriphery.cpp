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

bool UDevModeBotBrain::Tick_TitleLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UTitleNetworkHandler* TitleNet = GameInstance->GetTitleNetworkHandler();
        UTitleManager* Titles = GameInstance->GetTitleManager();
        if (!TitleNet || !Titles)
        {
            Finish(false, TEXT("no-titles"));
            return true;
        }
        if (!bTitleSent)
        {
            TitleNet->RequestEquipTitle(GameInstance->GetCurrentCharacterID(), TEXT("first_hunter"));
            bTitleSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT equip title first_hunter"));
            return true;
        }
        if (Titles->GetEquippedTitleSlug() == TEXT("first_hunter"))
        {
            BotAssert(true, TEXT("title-equipped"), 1, 1);
            BotShot(TEXT("title"));
            Step = EBotStep::EmoteLeg;
            CharLegStartTime = Now;
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("title-equipped"), 0, 1);
            Finish(false, TEXT("title-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_EmoteLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UEmoteNetworkHandler* EmoteNet = GameInstance->GetEmoteNetworkHandler();
        UEmoteManager* Emotes = GameInstance->GetEmoteManager();
        if (!EmoteNet || !Emotes)
        {
            Finish(false, TEXT("no-emotes"));
            return true;
        }
        if (!bEmoteSent)
        {
            // Play the first owned emote from the server snapshot — never
            // assume a slug is unlocked (salute is NOT default; wave is).
            const TArray<FEmoteDefinitionData>& Owned = Emotes->GetPlayerEmotesState().emotes;
            if (Owned.Num() == 0)
            {
                Finish(false, TEXT("no-emotes"));
                return true;
            }
            EmoteSlug = Owned[0].slug;
            EmoteNet->RequestUseEmote(EmoteSlug);
            bEmoteSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT emote %s sent"), *EmoteSlug);
            return true;
        }
        // The server broadcasts back (sender in own range): our actor plays
        // through the same PlayEmoteForCharacter path as any player.
        bool bPlaying = false;
        if (UEmoteComponent* Emote = Player->GetEmoteComponent())
        {
            bPlaying = Emote->IsPlayingEmote() && Emote->GetCurrentEmoteSlug() == EmoteSlug;
        }
        if (bPlaying)
        {
            BotAssert(true, TEXT("emote-playing"), 1, 1);
            BotShot(TEXT("emote"));
            Finish(Failures == 0, Failures == 0 ? TEXT("titles-emotes-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("emote-playing"), 0, 1);
            Finish(false, TEXT("emote-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_LocaleLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        ULocalizationSubsystem* Loc = GameInstance->GetSubsystem<ULocalizationSubsystem>();
        if (!Loc)
        {
            Finish(false, TEXT("no-locale"));
            return true;
        }
        SavedLocale = Loc->GetCurrentLocale();
        Loc->SetLocale(TEXT("ru"));
        const FText NameRu = Loc->GetItemDisplayName(TEXT("health_potion"));
        Loc->SetLocale(TEXT("en"));
        const FText NameEn = Loc->GetItemDisplayName(TEXT("health_potion"));
        BotAssert(!NameRu.IsEmpty(), TEXT("locale-ru"), 1, 1);
        BotAssert(!NameEn.IsEmpty(), TEXT("locale-en"), 1, 1);
        BotAssert(Loc->GetCurrentLocale() == TEXT("en"), TEXT("locale-switch"), 1, 1);
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT locale ru='%s' en='%s'"),
            *NameRu.ToString(), *NameEn.ToString());
        Loc->SetLocale(SavedLocale);
        BotAssert(Loc->GetCurrentLocale() == SavedLocale, TEXT("locale-restore"), 1, 1);
        BotShot(TEXT("locale"));
        // Camera before music: music needs designer BP content that may be
        // absent (loud FAIL below) — camera must still get verified.
        Step = EBotStep::CameraLeg;
        FlowLegStartTime = FPlatformTime::Seconds();
        return true;
}

bool UDevModeBotBrain::Tick_MusicLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UAudioManager* Audio = GameInstance->GetAudioManager();
        if (!Audio)
        {
            Finish(false, TEXT("no-audio"));
            return true;
        }
        if (Audio->ZoneMusicMap.Num() < 2)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT music zones mapped: %d (need 2)"), Audio->ZoneMusicMap.Num());
            Finish(false, TEXT("no-music-zones"));
            return true;
        }
        const FString ZoneA = Audio->ZoneMusicMap[0].ZoneName;
        const FString ListA = Audio->ZoneMusicMap[0].PlaylistId;
        const FString ZoneB = Audio->ZoneMusicMap[1].ZoneName;
        const FString ListB = Audio->ZoneMusicMap[1].PlaylistId;
        if (MusicStage == 0)
        {
            Audio->PlayMusicForZone(ZoneA);
            MusicStage = 1;
            return true;
        }
        if (MusicStage == 1 && Audio->GetActivePlaylistId() == ListA)
        {
            BotAssert(true, TEXT("music-zone-a"), 1, 1);
            Audio->PlayMusicForZone(ZoneB);
            MusicStage = 2;
            return true;
        }
        if (MusicStage == 2 && Audio->GetActivePlaylistId() == ListB)
        {
            BotAssert(true, TEXT("music-zone-b"), 1, 1);
            BotShot(TEXT("music"));
            Finish(Failures == 0, Failures == 0 ? TEXT("periphery-done") : TEXT("assert-failures"));
            return true;
        }
        if (FPlatformTime::Seconds() - FlowLegStartTime > FlowTimeoutSec)
        {
            BotAssert(false, TEXT("music-switch"), 0, 1);
            Finish(false, TEXT("music-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_CameraLeg(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Zoom through the real input path (OnScroll), assert clamp ends
        // and boom convergence. No server involved.
        const float ZoomMin = Player->GetZoomMin();
        const float ZoomMax = Player->GetZoomMax();
        if (ZoomStage == 0)
        {
            Player->OnScroll(FInputActionValue(100.0f));
            if (Player->GetDesiredZoom() <= ZoomMin + 1.0f)
            {
                BotAssert(true, TEXT("zoom-min"), 1, 1);
                ZoomStage = 1;
            }
            return true;
        }
        if (ZoomStage == 1)
        {
            Player->OnScroll(FInputActionValue(-100.0f));
            if (Player->GetDesiredZoom() >= ZoomMax - 1.0f)
            {
                BotAssert(true, TEXT("zoom-max"), 1, 1);
                ZoomStage = 2;
            }
            return true;
        }
        // Boom converges to the target length via Tick interp.
        if (FMath::Abs(Player->GetCameraArmLength() - ZoomMax) < 15.0f)
        {
            BotAssert(true, TEXT("zoom-converge"), 1, 1);
            BotShot(TEXT("camera"));
            Step = EBotStep::MusicLeg;
            FlowLegStartTime = FPlatformTime::Seconds();
            return true;
        }
        if (FPlatformTime::Seconds() - FlowLegStartTime > FlowTimeoutSec)
        {
            BotAssert(false, TEXT("zoom-converge"), 0, 1);
            Finish(false, TEXT("camera-timeout"));
        }
        return true;
}
