#pragma once

#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/PlayerController.h"

/**
 * Spawn a one-shot SFX with SoundClassOverride set BEFORE Play() is called.
 * SpawnSoundAtLocation calls Play() internally so any class override set afterwards
 * is ignored by the audio engine for that playback.
 *
 * Shared between BasicPlayer.cpp and BasicMOB.cpp.
 * Declared inline to avoid ODR violations when both TUs end up in the same unity file.
 *
 * @param MaxAudibleDistance  Sounds beyond this distance from the local player (cm)
 *                            are silently skipped. 0 = no distance check.
 */
inline UAudioComponent* SpawnSFXAttached(AActor* Owner, USoundBase* Sound,
    const FVector& WorldLocation, float VolumeMultiplier = 1.0f,
    USoundAttenuation* AttenuationSettings = nullptr,
    float MaxAudibleDistance = 3000.0f)
{
    if (!Owner || !Sound) { return nullptr; }

    // Distance gate: skip sounds from actors too far from the local player.
    // This prevents hearing mob combat audio from across the entire map.
    if (MaxAudibleDistance > 0.0f && Owner->GetWorld())
    {
        if (APlayerController* PC = Owner->GetWorld()->GetFirstPlayerController())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                const float Dist = FVector::Dist(Pawn->GetActorLocation(), WorldLocation);
                if (Dist > MaxAudibleDistance)
                {
                    return nullptr;
                }
            }
        }
    }

    USoundClass* SFXClass = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
    {
        if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
    }
    if (!SFXClass)
    {
        return UGameplayStatics::SpawnSoundAttached(
            Sound, Owner->GetRootComponent(), NAME_None,
            WorldLocation, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
            true, VolumeMultiplier, 1.0f, 0.0f, AttenuationSettings, nullptr, true);
    }
    UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
        Sound, Owner->GetRootComponent(), NAME_None,
        WorldLocation, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
        /*bStopWhenAttachedToDestroyed=*/true,
        VolumeMultiplier, 1.0f, 0.0f, AttenuationSettings, nullptr,
        /*bAutoActivate=*/false);
    if (AC)
    {
        AC->SoundClassOverride = SFXClass;
        AC->bAutoDestroy = true;
        AC->Play();
    }
    return AC;
}
