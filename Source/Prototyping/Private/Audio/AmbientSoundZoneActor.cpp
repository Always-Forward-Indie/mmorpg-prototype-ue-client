// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AmbientSoundZoneActor.h"
#include "Audio/AudioManager.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "MyGameInstance.h"
#include "TimerManager.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundAttenuation.h"

AAmbientSoundZoneActor::AAmbientSoundZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	RootComponent = TriggerBox;

	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->bAutoActivate = false;
	// Default: 2D, no spatialization. Applied in BeginPlay based on bSpatialized property.
	AudioComponent->bAllowSpatialization = false;
}

void AAmbientSoundZoneActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAttenuationSettings();
}

void AAmbientSoundZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AudioComponent)
	{
		AudioComponent->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void AAmbientSoundZoneActor::ApplyAttenuationSettings()
{
	if (!AudioComponent) { return; }

	if (bSpatialized)
	{
		const float SafeOuter = FMath::Max(AttenuationOuterRadius, AttenuationInnerRadius + 1.0f);
		AudioComponent->bAllowSpatialization = true;
		AudioComponent->bOverrideAttenuation = true;
		FSoundAttenuationSettings& Att  = AudioComponent->AttenuationOverrides;
		Att.bAttenuate                  = true;
		Att.bSpatialize                 = true;
		Att.AttenuationShape            = EAttenuationShape::Sphere;
		Att.AttenuationShapeExtents     = FVector(AttenuationInnerRadius, 0.f, 0.f);
		Att.FalloffDistance             = SafeOuter - AttenuationInnerRadius;
	}
	else
	{
		AudioComponent->bAllowSpatialization = false;
		AudioComponent->bOverrideAttenuation = false;
	}
}

void AAmbientSoundZoneActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' BeginPlay. AmbientSound=%s AudioComponent=%s"),
		*GetName(),
		AmbientSound ? *AmbientSound->GetName() : TEXT("NULL"),
		AudioComponent ? TEXT("OK") : TEXT("NULL"));

	// Route this zone through the Ambient SoundClass so the volume slider works.
	// Use explicit override if set, otherwise pull from AudioManager.
	USoundClass* TargetClass = SoundClassOverride;
	if (!TargetClass)
	{
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
		{
			if (GI->AudioManager) { TargetClass = GI->AudioManager->AmbientClass; }
		}
	}

	if (TargetClass && AudioComponent)
	{
		AudioComponent->SoundClassOverride = TargetClass;
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' SoundClass set to '%s'"),
			*GetName(), *TargetClass->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientZone] '%s' SoundClass NOT assigned! "
			"TargetClass=%s AudioComponent=%s — Ambient volume slider will NOT control this zone."),
			*GetName(),
			TargetClass ? *TargetClass->GetName() : TEXT("NULL"),
			AudioComponent ? TEXT("OK") : TEXT("NULL"));
	}

	if (AmbientSound && AudioComponent)
	{
		AudioComponent->SetSound(AmbientSound);
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' Sound assigned. VolumeMultiplier=%.2f bAllowSpatialization=%d"),
			*GetName(), VolumeMultiplier, (int32)AudioComponent->bAllowSpatialization);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientZone] '%s' Cannot assign sound: AmbientSound=%s AudioComponent=%s — zone will be SILENT."),
			*GetName(),
			AmbientSound ? *AmbientSound->GetName() : TEXT("NULL"),
			AudioComponent ? TEXT("OK") : TEXT("NULL"));
	}

	// Apply spatialization / attenuation settings (also called from OnConstruction for editor preview).
	ApplyAttenuationSettings();
	if (bSpatialized)
	{
		const float SafeOuter = FMath::Max(AttenuationOuterRadius, AttenuationInnerRadius + 1.0f);
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' Spatialized=true InnerR=%.0f OuterR=%.0f"),
			*GetName(), AttenuationInnerRadius, SafeOuter);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' Spatialized=false (2D playback)."), *GetName());
	}

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AAmbientSoundZoneActor::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AAmbientSoundZoneActor::OnBoxEndOverlap);

	// If bAutoPlay is set, start immediately (login screen, cinematic levels, etc.)
	// where there is no player pawn to walk into the trigger box.
	if (bAutoPlay)
	{
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' bAutoPlay=true — starting ambient immediately."), *GetName());
		bPlayerIsInside = true;
		StartAmbient();
		return; // skip the overlap poll entirely
	}

	// Warn if any scale component is negative — UE physics may not generate overlaps correctly.
	const FVector ScaledExtent = TriggerBox->GetScaledBoxExtent();
	if (ScaledExtent.X < 0.f || ScaledExtent.Y < 0.f || ScaledExtent.Z < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientZone] '%s' TriggerBox has NEGATIVE scaled extent (%s)! "
			"Check for negative scale on the actor or component — overlaps may not fire."),
			*GetName(), *ScaledExtent.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' TriggerBox extent=(%s). Starting overlap poll."),
			*GetName(), *ScaledExtent.ToString());
	}

	// Poll every 0.25s until the player pawn is spawned and overlaps have resolved.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			OverlapCheckTimerHandle, this,
			&AAmbientSoundZoneActor::CheckInitialOverlap, 0.25f, true);
	}
}

void AAmbientSoundZoneActor::CheckInitialOverlap()
{
	// Already playing — nothing to do, stop the timer.
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' CheckInitialOverlap: already playing, clearing timer."), *GetName());
		GetWorld()->GetTimerManager().ClearTimer(OverlapCheckTimerHandle);
		return;
	}

	// Look for ANY locally-controlled pawn in the overlapping set.
	// GetPlayerPawn(0) relies on standard UE possession and can return null
	// in MMO setups where the character is server-spawned and not yet possessed
	// through the standard PlayerController->Possess() path.
	TArray<AActor*> Overlapping;
	TriggerBox->GetOverlappingActors(Overlapping, APawn::StaticClass());

	APawn* LocalPawn = nullptr;
	for (AActor* A : Overlapping)
	{
		APawn* P = Cast<APawn>(A);
		if (P && P->IsLocallyControlled())
		{
			LocalPawn = P;
			break;
		}
	}

	// Also check via GetPlayerPawn as fallback (handles standard possession).
	if (!LocalPawn)
	{
		LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (LocalPawn && !Overlapping.Contains(LocalPawn))
		{
			LocalPawn = nullptr; // pawn exists but is outside the zone
		}
	}

	if (Overlapping.Num() == 0 && !UGameplayStatics::GetPlayerPawn(this, 0))
	{
		// No pawn in world yet — keep polling.
		UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' CheckInitialOverlap: no player pawn in world yet, polling..."), *GetName());
		return;
	}

	const bool bInside = (LocalPawn != nullptr);
	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' CheckInitialOverlap: InsideZone=%d OverlappingPawns=%d LocalPawn=%s — clearing timer."),
		*GetName(), (int32)bInside, Overlapping.Num(),
		LocalPawn ? *LocalPawn->GetName() : TEXT("None"));

	if (bInside)
	{
		bPlayerIsInside = true;
		StartAmbient();
	}
	GetWorld()->GetTimerManager().ClearTimer(OverlapCheckTimerHandle);
}
void AAmbientSoundZoneActor::StartAmbient()
{
	if (!AudioComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[AmbientZone] '%s' StartAmbient: AudioComponent is NULL!"), *GetName());
		return;
	}
	if (!AmbientSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientZone] '%s' StartAmbient: AmbientSound is NULL — assign a sound asset in Details."), *GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' StartAmbient: Sound='%s' FadeIn=%.2f Vol=%.2f SoundClass=%s"),
		*GetName(), *AmbientSound->GetName(), FadeInTime, VolumeMultiplier,
		AudioComponent->SoundClassOverride ? *AudioComponent->SoundClassOverride->GetName() : TEXT("None"));

	// Re-assign in case the asset was changed at runtime
	AudioComponent->SetSound(AmbientSound);

	if (FadeInTime > 0.0f)
	{
		// FadeIn(duration, targetVolume) starts playback automatically
		AudioComponent->FadeIn(FadeInTime, VolumeMultiplier);
	}
	else
	{
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		AudioComponent->Play();
	}

	// Bind loop handler — restarts the sound when it finishes if bLoop=true and player is still inside.
	// Clear first to avoid duplicate bindings on re-enter.
	AudioComponent->OnAudioFinished.RemoveDynamic(this, &AAmbientSoundZoneActor::OnAmbientFinished);
	AudioComponent->OnAudioFinished.AddDynamic(this, &AAmbientSoundZoneActor::OnAmbientFinished);

	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' StartAmbient: playback started (IsPlaying=%d) bLoop=%d."),
		*GetName(), (int32)AudioComponent->IsPlaying(), (int32)bLoop);
}

void AAmbientSoundZoneActor::OnAmbientFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' OnAmbientFinished: bLoop=%d bPlayerIsInside=%d."),
		*GetName(), (int32)bLoop, (int32)bPlayerIsInside);

	if (bLoop && bPlayerIsInside)
	{
		StartAmbient();
	}
}

void AAmbientSoundZoneActor::StopAmbient()
{
	if (!AudioComponent) { return; }

	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' StopAmbient: FadeOut=%.2f"), *GetName(), FadeOutTime);

	if (FadeOutTime > 0.0f)
	{
		AudioComponent->FadeOut(FadeOutTime, 0.0f);
	}
	else
	{
		AudioComponent->Stop();
	}
}

void AAmbientSoundZoneActor::OnBoxBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	APawn* P = Cast<APawn>(OtherActor);
	const bool bIsLocalPawn = P && P->IsLocallyControlled();

	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' OnBoxBeginOverlap: OtherActor='%s' IsPawn=%d IsLocallyControlled=%d"),
		*GetName(),
		OtherActor ? *OtherActor->GetName() : TEXT("NULL"),
		(int32)(P != nullptr),
		(int32)bIsLocalPawn);

	if (!bIsLocalPawn) { return; }

	bPlayerIsInside = true;
	StartAmbient();
}

void AAmbientSoundZoneActor::OnBoxEndOverlap(UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	APawn* P = Cast<APawn>(OtherActor);
	const bool bIsLocalPawn = P && P->IsLocallyControlled();

	UE_LOG(LogTemp, Log, TEXT("[AmbientZone] '%s' OnBoxEndOverlap: OtherActor='%s' IsPawn=%d IsLocallyControlled=%d"),
		*GetName(),
		OtherActor ? *OtherActor->GetName() : TEXT("NULL"),
		(int32)(P != nullptr),
		(int32)bIsLocalPawn);

	if (!bIsLocalPawn) { return; }

	bPlayerIsInside = false;
	StopAmbient();
}
