#include "UI/SkillBarWidget.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Utils/NetCore/SkillFeedbackRule.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Framework/Application/SlateApplication.h"

void USkillBarWidget::OnSkillSlotClicked(int32 SlotIndex, const FString& SkillSlug)
{
    CastSkillFromSlot(SlotIndex);
}

void USkillBarWidget::OnSkillSlotRightClicked(int32 SlotIndex, const FString& SkillSlug)
{
    UE_LOG(LogGameplay, Verbose, TEXT("SkillBarWidget: Right-clicked slot %d"), SlotIndex);
}

void USkillBarWidget::OnSkillDroppedOnSlot(int32 SlotIndex, const FPlayerSkillData& SkillData, const FKey& Hotkey)
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillBarWidget: Cannot handle skill drop - SkillManager not available"));
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

    UE_LOG(LogGameplay, Verbose, TEXT("SkillBarWidget: Dropped skill '%s' on slot %d"),
        *SkillData.networkData.skillSlug, SlotIndex);
}

void USkillBarWidget::OnSkillSlotDragCleared(int32 SlotIndex)
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillBarWidget: Cannot clear slot - SkillManager not available"));
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

    UE_LOG(LogGameplay, Verbose, TEXT("SkillBarWidget: Cleared slot %d via drag-out"), SlotIndex);
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
    UE_LOG(LogGameplay, Verbose, TEXT("SkillBarWidget: Player skills initialized, refreshing slots"));
    RefreshAllSlots();
}

void USkillBarWidget::OnSkillCooldownStarted(const FString& SkillSlug)
{
    bool bAssigned = false;
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug)
        {
            RefreshSlot(i);
            bAssigned = true;
        }
    }
    // Start blip: assigned-only, real cooldowns only (no GCD chirps, no login-restore burst).
    // Restore path (ApplyServerCooldowns) broadcasts the same event for visuals, but the
    // remaining is partial — we can't distinguish it here by event alone, so gate on duration:
    // short totals stay silent, which also kills restore-burst noise for short CDs.
    if (bAssigned && SkillManager && GameInstance && GameInstance->AudioManager)
    {
        const FPlayerSkillData SD = SkillManager->GetSkillData(SkillSlug);
        const float TotalSec = SD.networkData.cooldownMs > 0 ? SD.networkData.cooldownMs / 1000.0f : 0.0f;
        if (FSkillFeedbackRule::ShouldPlayStart(TotalSec, true, false))
        {
            GameInstance->AudioManager->PlayUISound(EUISoundEvent::SkillCooldownStart);
        }
    }
}

void USkillBarWidget::OnSkillReady(const FString& SkillSlug)
{
    bool bAssigned = false;
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug)
        {
            RefreshSlot(i);
            bAssigned = true;
        }
    }
    if (!bAssigned)
    {
        return;
    }

    // Per-skill ready sound, or fall back to global. Short CDs stay silent.
    if (SkillManager)
    {
        const FPlayerSkillData SkillData = SkillManager->GetSkillData(SkillSlug);
        const float TotalSec = SkillData.networkData.cooldownMs > 0 ? SkillData.networkData.cooldownMs / 1000.0f : 0.0f;
        if (!FSkillFeedbackRule::ShouldPlayReady(TotalSec, true))
        {
            return;
        }
        if (USoundBase* ReadyOverride = SkillData.definitionData.skillReadySound.Get())
        {
            if (GameInstance && GameInstance->AudioManager)
                GameInstance->AudioManager->PlayUISoundAsset(ReadyOverride);
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

