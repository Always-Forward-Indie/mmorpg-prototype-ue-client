#include "Animation/AnimNotify_PickupPoint.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_PickupPoint::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(MeshComp->GetAnimInstance()))
	{
		AnimInst->FirePickupPoint();
	}
}
