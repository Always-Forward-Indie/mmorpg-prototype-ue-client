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

USkillSlotWidget* USkillBarWidget::CreateSkillSlotWidget(int32 SlotIndex)
{
    if (!SkillSlotWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillBarWidget: SkillSlotWidgetClass not set"));
        return nullptr;
    }

    USkillSlotWidget* SlotWidget = CreateWidget<USkillSlotWidget>(this, SkillSlotWidgetClass);
    if (SlotWidget)
    {
        SlotWidget->SlotInitialize(SlotIndex, SkillManager);
        SlotWidget->SetOwnerBar(this);

        SlotWidget->OnSkillSlotClicked.AddDynamic(this, &USkillBarWidget::OnSkillSlotClicked);
        SlotWidget->OnSkillSlotRightClicked.AddDynamic(this, &USkillBarWidget::OnSkillSlotRightClicked);
        SlotWidget->OnSkillDroppedOnSlot.AddDynamic(this, &USkillBarWidget::OnSkillDroppedOnSlot);
        SlotWidget->OnSkillSlotDragCleared.AddDynamic(this, &USkillBarWidget::OnSkillSlotDragCleared);
    }

    return SlotWidget;
}

void USkillBarWidget::SetupDefaultHotkeys()
{
    if (!SkillManager)
    {
        return;
    }

    for (int32 i = 0; i < FMath::Min(SkillSlots.Num(), DefaultHotkeys.Num()); ++i)
    {
        FSkillSlotData SlotData = SkillManager->GetSkillSlot(i);
        if (!SlotData.boundKey.IsValid())
        {
            SkillManager->SetSkillSlot(i, SlotData.skillSlug, DefaultHotkeys[i]);
        }
    }
}

int32 USkillBarWidget::FindSlotByHotkey(const FKey& Hotkey) const
{
    if (!SkillManager)
    {
        return -1;
    }

    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        FSkillSlotData SlotData = SkillManager->GetSkillSlot(i);
        if (SlotData.boundKey == Hotkey)
        {
            return i;
        }
    }

    return -1;
}

void USkillBarWidget::UpdateSlotFromSkillData(int32 SlotIndex, const FPlayerSkillData& SkillData)
{
    if (SlotIndex >= 0 && SlotIndex < SkillSlots.Num())
    {
        USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
        if (SlotWidget)
        {
            SlotWidget->SetSkillData(SkillData);
        }
    }
}

void USkillBarWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
}
