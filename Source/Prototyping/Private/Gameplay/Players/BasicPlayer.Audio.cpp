// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "UI/UIManager.h"
#include "UI/NotificationToastWidget.h"
#include "UI/ChatWidget.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/ItemStruct.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Utils/PlayerAttributeParser.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/UI/CastBarWidget.h"
#include "Gameplay/Combat/BaseMMOProjectile.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Data/EffectDefinitionTable.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "Data/CharacterVisualData.h"
#include "Engine/AssetManager.h"

void ABasicPlayer::PlaySound(USoundBase* Sound)
{
    // Always re-stamp the SoundClass override so this component stays under
    // AudioManager volume control even if it was constructed before the
    // GameInstance had a valid SFXClass reference.
    if (MyGameInstance && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
    {
        AudioComponent->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
    }
    AudioComponent->SetSound(Sound);
    AudioComponent->Play();
}

// Stop sound
void ABasicPlayer::StopSound()
{
    AudioComponent->Stop();
}

const FEntityAudioProfile* ABasicPlayer::GetAudioProfile() const
{
    if (AudioProfileId.IsNone()) return nullptr;
    if (!MyGameInstance) return nullptr;
    if (UEntityAudioRepository* Repo = MyGameInstance->GetEntityAudioRepository())
    {
        return Repo->FindProfile(AudioProfileId);
    }
    return nullptr;
}

void ABasicPlayer::PreloadFootstepSounds()
{
    const FEntityAudioProfile* Profile = GetAudioProfile();
    if (!Profile || Profile->Footsteps.Num() == 0) return;

    FootstepSounds.Empty(Profile->Footsteps.Num());

    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
    TWeakObjectPtr<ABasicPlayer> WeakThis(this);

    for (const TSoftObjectPtr<USoundBase>& SoundSoft : Profile->Footsteps)
    {
        Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(),
            [WeakThis, SoundSoft]()
            {
                if (!WeakThis.IsValid()) return;
                if (USoundBase* Loaded = SoundSoft.Get())
                {
                    WeakThis->FootstepSounds.Add(Loaded);
                }
            });
    }
}

void ABasicPlayer::PlayEventSound(const TSoftObjectPtr<USoundBase>& SoundRef)
{
    if (SoundRef.IsNull()) return;
    USoundBase* Sound = SoundRef.LoadSynchronous();
    if (!Sound) return;

    USoundClass* SFXClass = (MyGameInstance && MyGameInstance->AudioManager)
        ? MyGameInstance->AudioManager->SFXClass : nullptr;

    USoundAttenuation* Attenuation = nullptr;
    if (const FEntityAudioProfile* Profile = GetAudioProfile())
    {
        if (!Profile->DefaultAttenuation.IsNull())
        {
            Attenuation = Profile->DefaultAttenuation.LoadSynchronous();
        }
    }

    if (SFXClass)
    {
        UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
            Sound,
            GetRootComponent(),
            NAME_None,
            GetActorLocation(),
            FRotator::ZeroRotator,
            EAttachLocation::KeepWorldPosition,
            /*bStopWhenAttachedToDestroyed=*/true,
            /*VolumeMultiplier=*/1.0f,
            /*PitchMultiplier=*/1.0f,
            /*StartTime=*/0.0f,
            Attenuation,
            /*ConcurrencySettings=*/nullptr,
            /*bAutoActivate=*/false);
        if (AC)
        {
            AC->SoundClassOverride = SFXClass;
            AC->bAutoDestroy = true;
            AC->Play();
        }
    }
    else
    {
        UGameplayStatics::SpawnSoundAttached(
            Sound, GetRootComponent(), NAME_None,
            GetActorLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
            true, 1.0f, 1.0f, 0.0f, Attenuation, nullptr, true);
    }
}

