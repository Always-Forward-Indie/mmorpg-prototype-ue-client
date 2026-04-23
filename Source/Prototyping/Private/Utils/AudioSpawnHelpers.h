#pragma once

#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"

/**
 * Spawn a one-shot SFX with SoundClassOverride set BEFORE Play() is called.
 * SpawnSoundAtLocation calls Play() internally so any class override set afterwards
 * is ignored by the audio engine for that playback.
 *
 * Shared between BasicPlayer.cpp and BasicMOB.cpp.
 * Declared inline to avoid ODR violations when both TUs end up in the same unity file.
 */
inline UAudioComponent* SpawnSFXAttached(AActor* Owner, USoundBase* Sound,
    const FVector& WorldLocation, float VolumeMultiplier = 1.0f)
{
    if (!Owner || !Sound) { return nullptr; }
    USoundClass* SFXClass = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
    {
        if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
    }
    if (!SFXClass)
    {
        return UGameplayStatics::SpawnSoundAtLocation(
            Owner, Sound, WorldLocation, FRotator::ZeroRotator, VolumeMultiplier);
    }
    UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
        Sound, Owner->GetRootComponent(), NAME_None,
        WorldLocation, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
        /*bStopWhenAttachedToDestroyed=*/true,
        VolumeMultiplier, 1.0f, 0.0f, nullptr, nullptr,
        /*bAutoActivate=*/false);
    if (AC)
    {
        AC->SoundClassOverride = SFXClass;
        AC->bAutoDestroy = true;
        AC->Play();
    }
    return AC;
}
