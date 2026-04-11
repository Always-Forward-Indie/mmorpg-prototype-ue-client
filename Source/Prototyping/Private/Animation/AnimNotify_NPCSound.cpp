#include "Animation/AnimNotify_NPCSound.h"
#include "Gameplay/NPCs/BasicNPC.h"

void UAnimNotify_NPCSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Animation*/,
	const FAnimNotifyEventReference& /*EventReference*/)
{
	if (!MeshComp) return;

	ABasicNPC* NPC = Cast<ABasicNPC>(MeshComp->GetOwner());
	if (!NPC) return;

	if (SoundSlotName == TEXT("Idle"))
	{
		NPC->PlayRandomIdleSound();
	}
	else
	{
		NPC->PlaySoundByName(SoundSlotName);
	}
}
