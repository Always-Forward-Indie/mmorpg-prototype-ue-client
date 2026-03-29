#include "Animation/AnimNotify_HitPoint.h"
#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_HitPoint::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(MeshComp->GetAnimInstance()))
	{
		AnimInst->FireHitPoint();
	}
}
