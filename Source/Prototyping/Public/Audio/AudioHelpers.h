#pragma once

#include "CoreMinimal.h"

// Shared by the AudioManager split (verbatim move from AudioManager.cpp).
// The settings-keys namespace and the GetWorldSafe helper are used by
// several chunks, so they cannot stay file-local. Both keep internal
// linkage per translation unit (`static` / named-namespace scope):
// stateless constants + a pure helper, no shared mutable state.

namespace AudioSettingsKeys
{
	static const FString MasterVolume  = TEXT("AudioMasterVolume");
	static const FString MusicVolume   = TEXT("AudioMusicVolume");
	static const FString SFXVolume     = TEXT("AudioSFXVolume");
	static const FString AmbientVolume = TEXT("AudioAmbientVolume");
	static const FString UIVolume      = TEXT("AudioUIVolume");
}

// ---------------------------------------------------------------------------
// Internal helper
// ---------------------------------------------------------------------------

static UWorld* GetWorldSafe(UObject* Ctx)
{
	return (GEngine && Ctx) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
}
