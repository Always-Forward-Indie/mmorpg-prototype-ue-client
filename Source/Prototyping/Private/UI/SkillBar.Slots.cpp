#include "UI/SkillBarWidget.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

void USkillBarWidget::BarInitialize(UMyGameInstance* InGameInstance)
{
    if (!InGameInstance)
    {
        UE_LOG(LogGameplay, Error, TEXT("SkillBarWidget: Cannot initialize with null GameInstance"));
        return;
    }

    GameInstance = InGameInstance;
    SkillManager = GameInstance->GetPlayerSkillManager();

    if (SkillManager)
    {
        SubscribeToSkillManagerEvents();
        UE_LOG(LogGameplay, Log, TEXT("SkillBarWidget: Initialized with PlayerSkillManager"));
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillBarWidget: PlayerSkillManager not available"));
    }

    CreateSkillSlots(DefaultNumSlots);
}

void USkillBarWidget::CreateSkillSlots(int32 NumSlots)
{
    if (!SkillSlotsContainer && !SkillGridContainer)
    {
        UE_LOG(LogGameplay, Error, TEXT("SkillBarWidget: No container widget found for skill slots"));
        return;
    }

    // Prevent double creation
    if (bSlotsCreated && SkillSlots.Num() == NumSlots)
    {
        return;
    }

    // Unbind events from old slot widgets before clearing
    for (USkillSlotWidget* OldSlot : SkillSlots)
    {
        if (OldSlot)
        {
            OldSlot->OnSkillSlotClicked.RemoveDynamic(this, &USkillBarWidget::OnSkillSlotClicked);
            OldSlot->OnSkillSlotRightClicked.RemoveDynamic(this, &USkillBarWidget::OnSkillSlotRightClicked);
            OldSlot->OnSkillDroppedOnSlot.RemoveDynamic(this, &USkillBarWidget::OnSkillDroppedOnSlot);
            OldSlot->OnSkillSlotDragCleared.RemoveDynamic(this, &USkillBarWidget::OnSkillSlotDragCleared);
        }
    }

    SkillSlots.Empty();
    
    if (SkillSlotsContainer)
    {
        SkillSlotsContainer->ClearChildren();
    }
    
    if (SkillGridContainer)
    {
        SkillGridContainer->ClearChildren();
    }

    for (int32 i = 0; i < NumSlots; ++i)
    {
        USkillSlotWidget* SlotWidget = CreateSkillSlotWidget(i);
        if (SlotWidget)
        {
            SkillSlots.Add(SlotWidget);

            if (bUseGridLayout && SkillGridContainer)
            {
                int32 Row = i / GridColumns;
                int32 Col = i % GridColumns;
                UUniformGridSlot* GridSlot = SkillGridContainer->AddChildToUniformGrid(SlotWidget, Row, Col);
                if (GridSlot)
                {
                    GridSlot->SetHorizontalAlignment(HAlign_Fill);
                    GridSlot->SetVerticalAlignment(VAlign_Fill);
                }
            }
            else if (SkillSlotsContainer)
            {
                UHorizontalBoxSlot* BoxSlot = SkillSlotsContainer->AddChildToHorizontalBox(SlotWidget);
                if (BoxSlot)
                {
                    BoxSlot->SetHorizontalAlignment(HAlign_Fill);
                    BoxSlot->SetVerticalAlignment(VAlign_Fill);
                    BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                }
            }
        }
    }

    SetupDefaultHotkeys();
    bSlotsCreated = true;
    UE_LOG(LogGameplay, Verbose, TEXT("SkillBarWidget: Created %d skill slots"), NumSlots);
}

void USkillBarWidget::RefreshAllSlots()
{
    if (!SkillManager)
    {
        return;
    }

    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        RefreshSlot(i);
    }
}

void USkillBarWidget::RefreshSlot(int32 SlotIndex)
{
    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        return;
    }

    USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
    if (!SlotWidget)
    {
        return;
    }

    FSkillSlotData SlotData = SkillManager->GetSkillSlot(SlotIndex);
    SlotWidget->SetSlotData(SlotData);
}

void USkillBarWidget::AssignSkillToSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& Hotkey)
{
    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillBarWidget: AssignSkillToSlot failed - invalid params (Slot: %d, SkillManager: %s)"),
            SlotIndex, SkillManager ? TEXT("valid") : TEXT("null"));
        return;
    }

    if (!SkillManager->HasSkill(SkillSlug))
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillBarWidget: Cannot assign unknown skill '%s' to slot %d"), *SkillSlug, SlotIndex);
        return;
    }

    SkillManager->SetSkillSlot(SlotIndex, SkillSlug, Hotkey);

    // Persist to server
    if (GameInstance)
    {
        if (UPlayerSkillNetworkHandler* NetHandler = GameInstance->GetPlayerSkillNetworkHandler())
        {
            NetHandler->SendSetSkillBarSlot(SlotIndex, SkillSlug, GameInstance->GetCurrentCharacterID());
        }
    }
}

void USkillBarWidget::ClearSlot(int32 SlotIndex)
{
    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        return;
    }

    SkillManager->SetSkillSlot(SlotIndex, "", FKey());
}

void USkillBarWidget::CastSkillFromSlot(int32 SlotIndex)
{
    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        return;
    }

    USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
    if (!SlotWidget || !SlotWidget->IsSlotAssigned())
    {
        return;
    }

    FString SkillSlug = SlotWidget->GetAssignedSkillSlug();

    // Check cooldown first so we can play the right feedback sound.
    if (SkillManager->IsSkillOnCooldown(SkillSlug))
    {
        // Per-skill override, or fall back to global AudioManager event.
        FPlayerSkillData SkillData = SkillManager->GetSkillData(SkillSlug);
        if (!SkillData.definitionData.skillOnCooldownSound.IsNull())
        {
            UGameplayStatics::PlaySound2D(this, SkillData.definitionData.skillOnCooldownSound.LoadSynchronous());
        }
        else if (GameInstance && GameInstance->AudioManager)
        {
            GameInstance->AudioManager->PlayUISound(EUISoundEvent::SkillCooldownStart);
        }
        return;
    }

    if (!SkillManager->CanCastSkill(SkillSlug))
    {
        // CanCastSkill failed for a reason other than cooldown (most likely not enough mana).
        FPlayerSkillData SkillData = SkillManager->GetSkillData(SkillSlug);
        if (!SkillData.definitionData.notEnoughManaSound.IsNull())
        {
            UGameplayStatics::PlaySound2D(this, SkillData.definitionData.notEnoughManaSound.LoadSynchronous());
        }
        else if (GameInstance && GameInstance->AudioManager)
        {
            GameInstance->AudioManager->PlayUISound(EUISoundEvent::SkillNotEnoughMana);
        }
        return;
    }

    bool bSuccess = SkillManager->TryCastSkill(SkillSlug, CurrentTargetId, CurrentTargetType);

    if (bSuccess)
    {
        OnSkillCast.Broadcast(SlotIndex, SkillSlug);
    }
}

void USkillBarWidget::TryCastSkillByHotkey(const FKey& Hotkey)
{
    int32 SlotIndex = FindSlotByHotkey(Hotkey);
    if (SlotIndex != -1)
    {
        CastSkillFromSlot(SlotIndex);
    }
}

void USkillBarWidget::SetCurrentTarget(int32 TargetId, ECasterType TargetType)
{
    CurrentTargetId = TargetId;
    CurrentTargetType = TargetType;
}

USkillSlotWidget* USkillBarWidget::GetSkillSlot(int32 SlotIndex) const
{
    if (SlotIndex >= 0 && SlotIndex < SkillSlots.Num())
    {
        return SkillSlots[SlotIndex];
    }
    return nullptr;
}

