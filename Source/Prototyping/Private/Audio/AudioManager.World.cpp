// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Audio/AudioHelpers.h"

// ---------------------------------------------------------------------------
// World (positional) sounds — one policy per class, data-driven.
// ---------------------------------------------------------------------------
//
// All 3D one-shots route through PlayWorldSound: distance gate, throttled LOS
// occlusion (volume duck + low-pass tail), class attenuation, SFX routing,
// voice cap. Reverb comes from the attenuation asset's send into placed
// AudioVolume actors — no code needed, designer places the volumes.

const FWorldSoundPreset& UAudioManager::DefaultWorldPreset(EWorldSoundClass SoundClass)
{
	// Footsteps carry less far; everything else shares the base 3 km gate.
	static const FWorldSoundPreset MobLike;
	static FWorldSoundPreset Close = []()
	{
		FWorldSoundPreset P;
		P.MaxAudibleDistance = 1500.0f;
		return P;
	}();
	switch (SoundClass)
	{
	case EWorldSoundClass::Footstep:
		return Close;
	default:
		return MobLike;
	}
}

const FWorldSoundPreset& UAudioManager::ResolveWorldPreset(EWorldSoundClass SoundClass) const
{
	for (const FWorldSoundClassEntry& Entry : WorldSoundPresets)
	{
		if (Entry.Class == SoundClass)
		{
			return Entry.Preset;
		}
	}
	return DefaultWorldPreset(SoundClass);
}

bool UAudioManager::CheckOccluded(AActor* Owner, const FVector& ListenerLoc,
	const FVector& Location, const FWorldSoundPreset& Preset)
{
	if (!Preset.bOcclusionEnabled)
	{
		return false;
	}
	UWorld* World = GetWorldSafe(WorldContextObject);
	if (!World)
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	if (const FOcclusionState* Cached = OcclusionCache.Find(Owner))
	{
		if (Now - Cached->LastCheckSec < static_cast<double>(FMath::Max(0.05f, Preset.OcclusionRefreshSec)))
		{
			return Cached->bOccluded;
		}
	}
	// Bound the cache: drop dead owners when it grows.
	if (OcclusionCache.Num() > 256)
	{
		TArray<TWeakObjectPtr<AActor>> Dead;
		for (const auto& Pair : OcclusionCache)
		{
			if (!Pair.Key.IsValid()) { Dead.Add(Pair.Key); }
		}
		for (const TWeakObjectPtr<AActor>& Key : Dead) { OcclusionCache.Remove(Key); }
	}
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AudioOcclusion), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Owner);
	if (Pawn) { Params.AddIgnoredActor(Pawn); }
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(
		Hit, ListenerLoc, Location, ECC_Visibility, Params);
	OcclusionCache.Add(Owner, FOcclusionState{ Now, bBlocked });
	return bBlocked;
}

UAudioComponent* UAudioManager::PlayWorldSound(EWorldSoundClass SoundClass, USoundBase* Sound,
	AActor* Owner, FVector Location, float VolumeScale, USoundAttenuation* AttenuationOverride)
{
	if (!Sound || !Owner)
	{
		return nullptr;
	}
	UWorld* World = GetWorldSafe(WorldContextObject);
	if (!World)
	{
		return nullptr;
	}
	const FWorldSoundPreset& Preset = ResolveWorldPreset(SoundClass);

	APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FVector ListenerLoc = Pawn ? Pawn->GetActorLocation() : Location;

	// Distance gate first: no trace, no voice for far owners.
	if (Preset.MaxAudibleDistance > 0.0f
		&& FVector::Dist(ListenerLoc, Location) > Preset.MaxAudibleDistance)
	{
		return nullptr;
	}

	float Volume = Preset.VolumeMultiplier * VolumeScale;
	const bool bOccluded = CheckOccluded(Owner, ListenerLoc, Location, Preset);
	if (bOccluded)
	{
		Volume *= Preset.OccludedVolumeMultiplier;
	}

	USoundAttenuation* Attenuation = AttenuationOverride ? AttenuationOverride : Preset.Attenuation;
	UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
		Sound, Owner->GetRootComponent(), NAME_None,
		Location, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
		/*bStopWhenAttachedToDestroyed=*/true,
		Volume, /*PitchMultiplier=*/1.0f, /*StartTime=*/0.0f,
		Attenuation, WorldConcurrency,
		/*bAutoDestroy=*/true);
	if (!AC)
	{
		return nullptr;
	}
	if (SFXClass)
	{
		AC->SoundClassOverride = SFXClass;
	}
	if (bOccluded && Preset.bOcclusionLowPass)
	{
		AC->bEnableLowPassFilter = true;
		AC->LowPassFilterFrequency = FMath::Clamp(Preset.OccludedLowPassFreq, 10.0f, 20000.0f);
	}
	AC->Play();
	return AC;
}
