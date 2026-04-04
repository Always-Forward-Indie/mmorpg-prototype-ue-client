#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "CombatCameraShake.generated.h"

/**
 * Procedural perlin-style camera shake pattern for combat hit feedback.
 * Implemented without the EngineCameras plugin dependency.
 * Duration: 0.25s — suitable for melee/ranged hits.
 * Driven by sine oscillators on pitch, yaw, and roll.
 */
UCLASS()
class PROTOTYPING_API UCombatCameraShakePattern : public UCameraShakePattern
{
    GENERATED_BODY()

public:
    UCombatCameraShakePattern(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
    virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
    virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
    virtual bool IsFinishedImpl() const override;
    virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;

private:
    float ElapsedTime   = 0.f;
    float TotalDuration = 0.25f;
    bool  bStopped      = false;
};

/**
 * Camera shake used for combat hit feedback.
 * Assign CombatCameraShakeClass in UIManager Blueprint,
 * or it is used automatically as C++ fallback.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UCombatCameraShake : public UCameraShakeBase
{
    GENERATED_BODY()

public:
    UCombatCameraShake(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
