#include "Animation/AnimNotify_EmoteEvent.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_EmoteEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner) return;

    UEmoteComponent* EmoteComp = Owner->FindComponentByClass<UEmoteComponent>();
    if (!EmoteComp) return;

    switch (Slot)
    {
        case EEmoteNotifySlot::PlaySound: EmoteComp->OnEmoteNotify_PlaySound(); break;
        case EEmoteNotifySlot::SpawnVFX:  EmoteComp->OnEmoteNotify_SpawnVFX();  break;
        case EEmoteNotifySlot::EmoteEnd:  EmoteComp->OnEmoteNotify_EmoteEnd();  break;
    }
}

FString UAnimNotify_EmoteEvent::GetNotifyName_Implementation() const
{
    const UEnum* Enum = StaticEnum<EEmoteNotifySlot>();
    if (Enum)
    {
        return FString::Printf(TEXT("Emote: %s"), *Enum->GetDisplayNameTextByValue((int64)Slot).ToString());
    }
    return TEXT("Emote Event");
}
