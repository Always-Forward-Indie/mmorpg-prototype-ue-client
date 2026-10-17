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

void USkillBarWidget::OnSkillSlotClicked(int32 SlotIndex, const FString& SkillSlug)
{
    CastSkillFromSlot(SlotIndex);
}

void USkillBarWidget::OnSkillSlotRightClicked(int32 SlotIndex, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Right-clicked slot %d"), SlotIndex);
}

void USkillBarWidget::OnSkillDroppedOnSlot(int32 SlotIndex, const FPlayerSkillData& SkillData, const FKey& Hotkey)
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Cannot handle skill drop - SkillManager not available"));
        return;
    }

    // Reset drag state on all slots to prevent stuck highlights
    for (USkillSlotWidget* SlotWidget : SkillSlots)
    {
        if (SlotWidget)
        {
            SlotWidget->ForceResetDragState();
        }
    }

    AssignSkillToSlot(SlotIndex, SkillData.networkData.skillSlug, Hotkey);

    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Dropped skill '%s' on slot %d"),
        *SkillData.networkData.skillSlug, SlotIndex);
}

void USkillBarWidget::OnSkillSlotDragCleared(int32 SlotIndex)
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Cannot clear slot - SkillManager not available"));
        return;
    }

    // Force-reset drag state on the source slot so it's immediately re-draggable
    // even if subsequent drop processing (OnSkillDroppedOnSlot) hasn't run yet.
    if (SlotIndex >= 0 && SlotIndex < SkillSlots.Num())
    {
        if (USkillSlotWidget* SourceSlot = SkillSlots[SlotIndex])
        {
            SourceSlot->ForceResetDragState();
        }
    }

    // Preserve the hotkey binding when clearing the skill
    FSkillSlotData SlotData = SkillManager->GetSkillSlot(SlotIndex);
    FKey BoundKey = SlotData.boundKey;

    SkillManager->SetSkillSlot(SlotIndex, "", BoundKey);

    // Persist clear to server
    if (GameInstance)
    {
        if (UPlayerSkillNetworkHandler* NetHandler = GameInstance->GetPlayerSkillNetworkHandler())
        {
            NetHandler->SendSetSkillBarSlot(SlotIndex, TEXT(""), GameInstance->GetCurrentCharacterID());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Cleared slot %d via drag-out"), SlotIndex);
}

void USkillBarWidget::ClearHighlightOnOtherSlots(USkillSlotWidget* Except)
{
    for (USkillSlotWidget* SlotWidget : SkillSlots)
    {
        if (SlotWidget && SlotWidget != Except && SlotWidget->IsDropHighlighted())
        {
            SlotWidget->SetDropHighlighted(false);
        }
    }
}

void USkillBarWidget::OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills)
{
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Player skills initialized, refreshing slots"));
    RefreshAllSlots();
}

void USkillBarWidget::OnSkillCooldownStarted(const FString& SkillSlug)
{
    UE_LOG(LogTemp, Warning,
        TEXT("SkillBarWidget: OnSkillCooldownStarted '%s' — slots=%d"),
        *SkillSlug, SkillSlots.Num());
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        UE_LOG(LogTemp, Warning,
            TEXT("SkillBarWidget:   slot[%d] assigned='%s' match=%d"),
            i,
            SlotWidget ? *SlotWidget->GetAssignedSkillSlug() : TEXT("NULL"),
            (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug) ? 1 : 0);
        if (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug)
        {
            RefreshSlot(i);
        }
    }
    if (GameInstance && GameInstance->AudioManager)
    {
        GameInstance->AudioManager->PlayUISound(EUISoundEvent::SkillCooldownStart);
    }
}

void USkillBarWidget::OnSkillReady(const FString& SkillSlug)
{
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug)
        {
            RefreshSlot(i);
        }
    }

    // Per-skill ready sound, or fall back to global.
    if (SkillManager)
    {
        FPlayerSkillData SkillData = SkillManager->GetSkillData(SkillSlug);
        if (!SkillData.definitionData.skillReadySound.IsNull())
        {
            UGameplayStatics::PlaySound2D(this, SkillData.definitionData.skillReadySound.LoadSynchronous());
            return;
        }
    }
    if (GameInstance && GameInstance->AudioManager)
    {
        GameInstance->AudioManager->PlayUISound(EUISoundEvent::SkillReady);
    }
}

void USkillBarWidget::OnSkillSlotChanged(int32 SlotIndex, const FSkillSlotData& SlotData)
{
    RefreshSlot(SlotIndex);
}

