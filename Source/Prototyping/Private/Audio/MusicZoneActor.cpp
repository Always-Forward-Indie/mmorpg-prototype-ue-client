// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/MusicZoneActor.h"
#include "Audio/AudioManager.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MyGameInstance.h"
#include "TimerManager.h"

AMusicZoneActor::AMusicZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	RootComponent = TriggerBox;
}

void AMusicZoneActor::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMusicZoneActor::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AMusicZoneActor::OnBoxEndOverlap);

	if (PlaylistId.IsEmpty()) { return; }

	if (bTriggerWithoutPawn)
	{
		// Login level path: no pawn needed — start the playlist right away.
		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		if (GI && GI->AudioManager)
		{
			GI->AudioManager->ReapplySoundMix();
			GI->AudioManager->PlayPlaylist(PlaylistId, bForceRestartOnEnter);
		}
		return;
	}

	// Game world path: poll every 0.25 s until the local pawn is present and
	// physically inside the box (handles spawning inside a zone on level load).
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			OverlapCheckTimerHandle, this,
			&AMusicZoneActor::CheckInitialOverlap, 0.25f, true);
	}
}

void AMusicZoneActor::CheckInitialOverlap()
{
	if (PlaylistId.IsEmpty())
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
		bIsPlayerInside = true;

		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		if (GI && GI->AudioManager)
		{
			// Ensure the SoundMix is active in the current world before starting music.
			// The first time a player spawns inside a zone the SoundMix might not be
			// pushed yet (world just became valid); ReapplySoundMix() is a no-op if
			// it was already applied, so calling it here is always safe.
			GI->AudioManager->ReapplySoundMix();
			GI->AudioManager->PlayPlaylist(PlaylistId, bForceRestartOnEnter);
		}
		GetWorld()->GetTimerManager().ClearTimer(OverlapCheckTimerHandle);
	}
}

void AMusicZoneActor::OnBoxBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!OtherActor) { return; }

	// Only react to the local player's pawn.
	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor != LocalPawn) { return; }

	bIsPlayerInside = true;

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (GI && GI->AudioManager && !PlaylistId.IsEmpty())
	{
		GI->AudioManager->PlayPlaylist(PlaylistId, bForceRestartOnEnter);
	}
}

void AMusicZoneActor::OnBoxEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!OtherActor) { return; }

	APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor != LocalPawn) { return; }

	// Verify the pawn is genuinely outside the box before stopping music.
	// UE can fire a spurious EndOverlap during Actor spawn, Possess, or capsule
	// resize even when the pawn never actually left the trigger volume.
	TArray<AActor*> StillOverlapping;
	TriggerBox->GetOverlappingActors(StillOverlapping);
	if (StillOverlapping.Contains(LocalPawn))
	{
		// Physics says pawn is still inside — ignore this phantom EndOverlap.
		return;
	}

	bIsPlayerInside = false;

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (GI && GI->AudioManager)
	{
		GI->AudioManager->StopMusic(FadeOutTimeOverride);
	}
}

void AMusicZoneActor::OnPlayerSpawned()
{
	// Called by GameInstance right after the local pawn is possessed.
	// Fire the overlap check immediately so we don't have to wait for the
	// next 0.25 s timer tick — the pawn and its physics overlap are ready now.
	if (bTriggerWithoutPawn || PlaylistId.IsEmpty()) { return; }
	CheckInitialOverlap();
}
