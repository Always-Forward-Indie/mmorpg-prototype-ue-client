#pragma once

#include "CoreMinimal.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"

// Shared by the DroppedItemActor split (one definition, many TUs).
// Moved verbatim from DroppedItemActor.cpp: the one-shot SFX helper is used
// by both Visuals and Pickup chunks, and the LogDropSnap category by
// Lifecycle/BeginPlayTick/Trajectory/Ground chunks, so neither can stay
// file-local. `inline` / DECLARE+DEFINE keep a single instance.
DECLARE_LOG_CATEGORY_EXTERN(LogDropSnap, Log, All);

// Spawn a one-shot SFX with SoundClassOverride set BEFORE Play() — same pattern
// used across the whole codebase.  SpawnSoundAtLocation/PlaySoundAtLocation call
// Play() internally so any class override set afterwards is silently ignored.
inline void PlayItemSFX(AActor* Owner, USoundBase* Sound, USoundAttenuation* Attenuation = nullptr)
{
	if (!Owner || !Sound) { return; }

	USoundClass* SFXClass = nullptr;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
	{
		if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
	}

	if (SFXClass)
	{
		UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
			Sound, Owner->GetRootComponent(), NAME_None,
			Owner->GetActorLocation(), FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition,
			/*bStopWhenAttachedToDestroyed=*/true,
			1.0f, 1.0f, 0.0f, Attenuation, nullptr,
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
		UGameplayStatics::SpawnSoundAttached(Sound, Owner->GetRootComponent(), NAME_None,
			Owner->GetActorLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
			true, 1.0f, 1.0f, 0.0f, Attenuation, nullptr, true);
	}
}
