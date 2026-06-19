// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/MusicZoneActor.h"
#include "Audio/AudioManager.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
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

	TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &AMusicZoneActor::OnBoxBeginOverlap);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMusicZoneActor::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.RemoveDynamic(this, &AMusicZoneActor::OnBoxEndOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AMusicZoneActor::OnBoxEndOverlap);

	if (PlaylistId.IsEmpty()) { return; }

	if (bTriggerWithoutPawn)
	{
		// Login level path: no pawn needed � start the playlist right away.
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

	// Guard against spurious EndOverlap during Actor spawn, Possess,
	// or capsule resize — physics must confirm the pawn is genuinely outside.
	TArray<AActor*> StillOverlapping;
	TriggerBox->GetOverlappingActors(StillOverlapping);
	if (StillOverlapping.Contains(LocalPawn))
	{
		return;
	}

	bIsPlayerInside = false;

	if (PlaylistId.IsEmpty()) { return; }

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI || !GI->AudioManager) { return; }

	// Layer 1: If another zone already changed the active playlist, do not
	// interfere — its BeginOverlap handled the transition before our EndOverlap.
	if (GI->AudioManager->GetActivePlaylistId() != PlaylistId)
	{
		return;
	}

	// Layer 2: If the player is inside ANY other MusicZoneActor with a
	// playlist set, that zone's BeginOverlap (past or pending) owns the
	// music decision. We must not stop the music — regardless of whether
	// the other zone's playlist matches ours.
	if (IsPlayerInAnyMusicZone(LocalPawn))
	{
		return;
	}

	// No other zone covers the player — safe to stop with fade-out.
	GI->AudioManager->StopMusic(FadeOutTimeOverride);
}

void AMusicZoneActor::OnPlayerSpawned()
{
	if (bTriggerWithoutPawn || PlaylistId.IsEmpty()) { return; }
	CheckInitialOverlap();
}

bool AMusicZoneActor::IsPlayerInAnyMusicZone(APawn* LocalPawn) const
{
	if (!LocalPawn) { return false; }

	TArray<AActor*> Overlapping;
	LocalPawn->GetOverlappingActors(Overlapping, AMusicZoneActor::StaticClass());
	for (AActor* A : Overlapping)
	{
		AMusicZoneActor* Zone = Cast<AMusicZoneActor>(A);
		if (Zone && Zone != this && !Zone->PlaylistId.IsEmpty())
		{
			return true;
		}
	}

	for (TActorIterator<AMusicZoneActor> It(GetWorld()); It; ++It)
	{
		AMusicZoneActor* Zone = *It;
		if (Zone && Zone != this
			&& !Zone->PlaylistId.IsEmpty()
			&& Zone->TriggerBox
			&& Zone->TriggerBox->IsOverlappingActor(LocalPawn))
		{
			return true;
		}
	}

	return false;
}
