#include "Gameplay/LoginLevel/LoginLevelSetupActor.h"
#include "Components/ArrowComponent.h"

ALoginLevelSetupActor::ALoginLevelSetupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// ─── Camera spots ─────────────────────────────────────────────────────

	LoginCameraSpot = CreateDefaultSubobject<UArrowComponent>(TEXT("LoginCameraSpot"));
	LoginCameraSpot->SetupAttachment(Root);
	LoginCameraSpot->ArrowColor = FColor::Blue;
	LoginCameraSpot->ArrowSize  = 2.0f;

	SelectCameraSpot = CreateDefaultSubobject<UArrowComponent>(TEXT("SelectCameraSpot"));
	SelectCameraSpot->SetupAttachment(Root);
	SelectCameraSpot->ArrowColor = FColor::Cyan;
	SelectCameraSpot->ArrowSize  = 2.0f;

	CreateCameraSpot = CreateDefaultSubobject<UArrowComponent>(TEXT("CreateCameraSpot"));
	CreateCameraSpot->SetupAttachment(Root);
	CreateCameraSpot->ArrowColor = FColor::Green;
	CreateCameraSpot->ArrowSize  = 2.0f;

	// ─── Character slots ──────────────────────────────────────────────────

	static const TCHAR* SlotNames[] = {
		TEXT("PodiumSlot_0"), TEXT("PodiumSlot_1"),
		TEXT("PodiumSlot_2"), TEXT("PodiumSlot_3")
	};
	for (int32 i = 0; i < 4; ++i)
	{
		UArrowComponent* Slot = CreateDefaultSubobject<UArrowComponent>(SlotNames[i]);
		Slot->SetupAttachment(Root);
		Slot->ArrowColor = FColor::Yellow;
		Slot->ArrowSize  = 1.5f;
		PodiumSlots.Add(Slot);
	}

	CreateSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("CreateSlot"));
	CreateSlot->SetupAttachment(Root);
	CreateSlot->ArrowColor = FColor(255, 140, 0); // Orange
	CreateSlot->ArrowSize  = 1.5f;

	SelectedCharacterSlot = CreateDefaultSubobject<UArrowComponent>(TEXT("SelectedCharacterSlot"));
	SelectedCharacterSlot->SetupAttachment(Root);
	SelectedCharacterSlot->ArrowColor = FColor::Red;
	SelectedCharacterSlot->ArrowSize  = 1.5f;
}
