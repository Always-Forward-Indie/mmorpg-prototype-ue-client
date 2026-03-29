#include "Animation/AnimNotify_CameraShake.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"

UAnimNotify_CameraShake::UAnimNotify_CameraShake()
{
}

void UAnimNotify_CameraShake::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	ABasicPlayer* Player = Cast<ABasicPlayer>(MeshComp->GetOwner());
	if (!Player) return;

	// Only fire on the locally controlled player — remote players don't shake their own camera
	if (Player->IsLocallyControlled())
	{
		if (UUIManager* UI = Player->GetUIManager())
		{
			UI->PlayCombatCameraShake(ShakeIntensity);
		}
	}
}
