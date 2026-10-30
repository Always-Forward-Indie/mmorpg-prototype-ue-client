#include "Audio/UISlateSoundDevice.h"
#include "Audio/AudioManager.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

FUIMixSlateSoundDevice::FUIMixSlateSoundDevice(UAudioManager* InAudioManager)
    : AudioManager(InAudioManager)
{
}

USoundBase* FUIMixSlateSoundDevice::ResolveSoundBase(const FSlateSound& Sound)
{
    return Cast<USoundBase>(Sound.GetResourceObject());
}

void FUIMixSlateSoundDevice::PlaySound(const FSlateSound& Sound, int32 UserIndex) const
{
    USoundBase* SoundBase = ResolveSoundBase(Sound);
    if (!SoundBase) return;

    if (UAudioManager* Manager = AudioManager.Get())
    {
        Manager->PlayUISoundAsset(SoundBase);
        return;
    }

    // No game audio manager (editor chrome outside a game world): direct
    // playback, exactly today's behavior.
    UWorld* World = nullptr;
    if (GEngine && GEngine->GameViewport)
    {
        World = GEngine->GameViewport->GetWorld();
    }
    if (World)
    {
        UGameplayStatics::PlaySound2D(World, SoundBase);
    }
    (void)UserIndex;
}

float FUIMixSlateSoundDevice::GetSoundDuration(const FSlateSound& Sound) const
{
    if (USoundBase* SoundBase = ResolveSoundBase(Sound))
    {
        return SoundBase->GetDuration();
    }
    return 0.0f;
}
