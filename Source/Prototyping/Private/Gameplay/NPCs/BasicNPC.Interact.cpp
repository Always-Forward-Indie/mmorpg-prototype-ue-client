#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/NPCs/NPCAnimInstance.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/UI/NPCNameplateComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Data/EntityAudioRepository.h"
#include "GameFramework/Pawn.h"

void ABasicNPC::OnPlayerInteract(APlayerController* InteractingPlayer)
{
	if (!NPCData.isInteractable)
	{
		UE_LOG(LogTemp, Warning, TEXT("NPC %s is not interactable"), *NPCData.name);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Player interacting with NPC %s"), *NPCData.name);
	
	// Play interaction sound if available
	PlaySoundByName("Interact");

	// Trigger greeting animation + talking state
	PlayGreetingSound();
	
	// Call Blueprint event
	OnInteractionReceived(InteractingPlayer);
}

void ABasicNPC::NotifyDialogueClosed()
{
	// Legacy entry-point kept for Blueprint/external callers.
	// Internally delegates to the counted path.
	NotifyWindowClosed();
}

void ABasicNPC::NotifyWindowOpened()
{
	++ActiveInteractionWindowCount;
	// Cancel any scheduled farewell – a new window opened for this NPC.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FarewellTimer);
	}
}

void ABasicNPC::NotifyWindowClosed()
{
	ActiveInteractionWindowCount = FMath::Max(0, ActiveInteractionWindowCount - 1);

	if (ActiveInteractionWindowCount > 0)
	{
		return; // Other windows still open – no farewell yet.
	}

	// All windows closed. Use a short deferred timer so a rapid dialogue→shop
	// transition (where DIALOGUE_CLOSE arrives before vendor_shop_data) doesn't
	// fire the farewell prematurely. 250 ms is imperceptible but sufficient.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			FarewellTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (ActiveInteractionWindowCount <= 0)
				{
					PlayFarewellSound();
				}
			}),
			0.25f,
			/*bLooping=*/false);
	}
}

