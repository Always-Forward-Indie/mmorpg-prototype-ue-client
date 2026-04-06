#include "Animation/AnimNotify_PlayerCombatEvent.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Components/SkeletalMeshComponent.h"

FString UAnimNotify_PlayerCombatEvent::GetNotifyName_Implementation() const
{
    switch (Slot)
    {
    case ECombatSoundSlot::SwingSound:   return TEXT("Combat: Swing Sound");
    case ECombatSoundSlot::VoiceAttack:  return TEXT("Combat: Voice Attack");
    case ECombatSoundSlot::CastRelease:  return TEXT("Combat: Cast Release");
    }
    return TEXT("Player Combat Event");
}

void UAnimNotify_PlayerCombatEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    ABasicPlayer* Player = Cast<ABasicPlayer>(MeshComp->GetOwner());
    if (!Player) return;

    Player->PlayCombatSoundEvent(Slot);
}
