#pragma once

#include "CoreMinimal.h"
#include "Sound/ISlateSoundDevice.h"

class UAudioManager;
class USoundBase;

/**
 * FUIMixSlateSoundDevice
 *
 * Slate sound device that routes every Slate UI sound (button PressedSlateSound
 * in all WBP styles, etc.) through UAudioManager, so the UI volume slider and
 * the sound mix actually apply. Without this, Slate sounds play on the raw
 * audio device and ignore every slider — including Master.
 *
 * When the AudioManager is gone (editor chrome outside a game world) it falls
 * back to a direct PlaySound2D — today's behavior, no regression.
 *
 * Installed by UAudioManager::Init / ReapplySoundMix via
 * FSlateApplication::InitializeSound.
 *
 * Multi-PIE note: one SlateApplication per process, so clicks from every PIE
 * instance route through the most recently installed manager. Volume follows
 * that client's sliders; playback is process-wide either way.
 */
class PROTOTYPING_API FUIMixSlateSoundDevice : public ISlateSoundDevice
{
public:
    explicit FUIMixSlateSoundDevice(UAudioManager* InAudioManager);

    virtual void PlaySound(const FSlateSound& Sound, int32 UserIndex = 0) const override;
    virtual float GetSoundDuration(const FSlateSound& Sound) const override;

private:
    static USoundBase* ResolveSoundBase(const FSlateSound& Sound);

    TWeakObjectPtr<UAudioManager> AudioManager;
};
