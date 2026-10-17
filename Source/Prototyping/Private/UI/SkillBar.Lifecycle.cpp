#include "UI/SkillBarWidget.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

USkillBarWidget::USkillBarWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    GameInstance = nullptr;
    SkillManager = nullptr;
    CurrentTargetId = 0;
    CurrentTargetType = ECasterType::None;
    DefaultNumSlots = 10;
    bUseGridLayout = false;
    GridColumns = 5;

    DefaultHotkeys.Add(EKeys::One);
    DefaultHotkeys.Add(EKeys::Two);
    DefaultHotkeys.Add(EKeys::Three);
    DefaultHotkeys.Add(EKeys::Four);
    DefaultHotkeys.Add(EKeys::Five);
    DefaultHotkeys.Add(EKeys::Six);
    DefaultHotkeys.Add(EKeys::Seven);
    DefaultHotkeys.Add(EKeys::Eight);
    DefaultHotkeys.Add(EKeys::Nine);
    DefaultHotkeys.Add(EKeys::Zero);
}

void USkillBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Slots are created only once via BarInitialize -> CreateSkillSlots.
    // NativeConstruct may fire multiple times (widget added/removed from viewport),
    // so we must NOT create slots here to avoid duplicates.
    // Re-subscribe to events if SkillManager is already set (e.g., widget re-added).
    if (SkillManager && !bEventsSubscribed)
    {
        SubscribeToSkillManagerEvents();
    }

    // Container widgets must be SelfHitTestInvisible so they do NOT intercept
    // drag-and-drop events. Slate routes DragOver/Drop to the deepest hit-testable
    // widget under the cursor. If the Overlay or HorizontalBox is Visible it
    // captures the hit before child SkillSlotWidgets, and NativeOnDrop never fires
    // on the slots.
    if (SkillBarContainerOverlay)
    {
        SkillBarContainerOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    if (SkillSlotsContainer)
    {
        SkillSlotsContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    if (SkillGridContainer)
    {
        SkillGridContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget::NativeConstruct — containers set SelfHitTestInvisible, slots=%d eventsSub=%d"),
        SkillSlots.Num(), bEventsSubscribed ? 1 : 0);
}

void USkillBarWidget::NativeDestruct()
{
    UnsubscribeFromSkillManagerEvents();
    Super::NativeDestruct();
}

FReply USkillBarWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    FKey PressedKey = InKeyEvent.GetKey();
    
    int32 SlotIndex = FindSlotByHotkey(PressedKey);
    if (SlotIndex != -1)
    {
        CastSkillFromSlot(SlotIndex);
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

