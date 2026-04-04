#include "Gameplay/Combat/CombatCameraShake.h"
#include "Math/UnrealMathUtility.h"

// ---------------------------------------------------------------------------
// UCombatCameraShakePattern
// ---------------------------------------------------------------------------

UCombatCameraShakePattern::UCombatCameraShakePattern(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UCombatCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
    OutInfo.Duration  = FCameraShakeDuration(TotalDuration);
    OutInfo.BlendIn   = 0.02f;
    OutInfo.BlendOut  = 0.15f;
}

void UCombatCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
    ElapsedTime = 0.f;
    bStopped    = false;
}

void UCombatCameraShakePattern::UpdateShakePatternImpl(
    const FCameraShakePatternUpdateParams& Params,
    FCameraShakePatternUpdateResult& OutResult)
{
    ElapsedTime += Params.DeltaTime;

    const float Scale = Params.GetTotalScale();

    // Blend envelope: quick fade-in, smooth fade-out
    float Envelope = 1.0f;
    const float BlendIn  = 0.02f;
    const float BlendOut = 0.15f;
    if (ElapsedTime < BlendIn)
    {
        Envelope = ElapsedTime / BlendIn;
    }
    else if (ElapsedTime > TotalDuration - BlendOut)
    {
        Envelope = FMath::Max(0.f, (TotalDuration - ElapsedTime) / BlendOut);
    }

    const float T = ElapsedTime;

    // Sine oscillators — fast frequency for snappy hit feel
    const float PitchAmp = 1.5f * Scale * Envelope;
    const float YawAmp   = 1.0f * Scale * Envelope;
    const float RollAmp  = 0.5f * Scale * Envelope;

    OutResult.Rotation.Pitch = PitchAmp * FMath::Sin(T * 18.0f * UE_TWO_PI);
    OutResult.Rotation.Yaw   = YawAmp   * FMath::Sin(T * 20.0f * UE_TWO_PI + 1.1f);
    OutResult.Rotation.Roll  = RollAmp  * FMath::Sin(T * 15.0f * UE_TWO_PI + 2.3f);
}

bool UCombatCameraShakePattern::IsFinishedImpl() const
{
    return bStopped || (ElapsedTime >= TotalDuration);
}

void UCombatCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
    if (Params.bImmediately)
    {
        bStopped = true;
    }
}

// ---------------------------------------------------------------------------
// UCombatCameraShake
// ---------------------------------------------------------------------------

UCombatCameraShake::UCombatCameraShake(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bSingleInstance = false;

    UCombatCameraShakePattern* Pattern =
        CreateDefaultSubobject<UCombatCameraShakePattern>(TEXT("CombatShakePattern"));
    SetRootShakePattern(Pattern);
}
