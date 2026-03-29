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
}

void AAmbientSoundZoneActor::BeginPlay()
{
	Super::BeginPlay();

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
	}

	if (AmbientSound && AudioComponent)
	{
		AudioComponent->SetSound(AmbientSound);
		AudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	}

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AAmbientSoundZoneActor::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AAmbientSoundZoneActor::OnBoxEndOverlap);

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
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		GetWorld()->GetTimerManager().ClearTimer(OverlapCheckTimerHandle);
		return;
	}

	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!LocalPawn) { return; }

	TArray<AActor*> Overlapping;
	TriggerBox->GetOverlappingActors(Overlapping);
	if (Overlapping.Contains(LocalPawn))
	{
		StartAmbient();
		GetWorld()->GetTimerManager().ClearTimer(OverlapCheckTimerHandle);
	}
}
void AAmbientSoundZoneActor::StartAmbient()
{
	if (!AudioComponent || !AmbientSound) { return; }

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
}

void AAmbientSoundZoneActor::StopAmbient()
{
	if (!AudioComponent) { return; }

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
	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor != LocalPawn) { return; }

	StartAmbient();
}

void AAmbientSoundZoneActor::OnBoxEndOverlap(UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor != LocalPawn) { return; }

	StopAmbient();
}
