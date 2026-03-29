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

	// Poll every 0.25s until the player pawn is spawned and overlaps have resolved.
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
		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		if (GI && GI->AudioManager)
		{
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

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (GI && GI->AudioManager)
	{
		GI->AudioManager->StopMusic(FadeOutTimeOverride);
	}
}
