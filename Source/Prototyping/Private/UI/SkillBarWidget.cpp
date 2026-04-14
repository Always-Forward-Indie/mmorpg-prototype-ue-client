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

void USkillBarWidget::BarInitialize(UMyGameInstance* InGameInstance)
{
    if (!InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillBarWidget: Cannot initialize with null GameInstance"));
        return;
    }

    GameInstance = InGameInstance;
    SkillManager = GameInstance->GetPlayerSkillManager();

    if (SkillManager)
    {
        SubscribeToSkillManagerEvents();
        UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Initialized with PlayerSkillManager"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: PlayerSkillManager not available"));
    }

    CreateSkillSlots(DefaultNumSlots);
}

void USkillBarWidget::CreateSkillSlots(int32 NumSlots)
{
    if (!SkillSlotsContainer && !SkillGridContainer)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillBarWidget: No container widget found for skill slots"));
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
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Created %d skill slots"), NumSlots);
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
        UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: AssignSkillToSlot failed - invalid params (Slot: %d, SkillManager: %s)"),
            SlotIndex, SkillManager ? TEXT("valid") : TEXT("null"));
        return;
    }

    if (!SkillManager->HasSkill(SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Cannot assign unknown skill '%s' to slot %d"), *SkillSlug, SlotIndex);
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

void USkillBarWidget::OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills)
{
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Player skills initialized, refreshing slots"));
    RefreshAllSlots();
}

void USkillBarWidget::OnSkillCooldownStarted(const FString& SkillSlug)
{
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
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

void USkillBarWidget::SubscribeToSkillManagerEvents()
{
    if (!SkillManager || bEventsSubscribed)
    {
        return;
    }

    SkillManager->OnSkillsInitialized.AddDynamic(this, &USkillBarWidget::OnPlayerSkillsInitialized);
    SkillManager->OnSkillCooldownStarted.AddDynamic(this, &USkillBarWidget::OnSkillCooldownStarted);
    SkillManager->OnSkillReady.AddDynamic(this, &USkillBarWidget::OnSkillReady);
    SkillManager->OnSkillSlotChanged.AddDynamic(this, &USkillBarWidget::OnSkillSlotChanged);

    bEventsSubscribed = true;
}

void USkillBarWidget::UnsubscribeFromSkillManagerEvents()
{
    if (!SkillManager || !bEventsSubscribed)
    {
        return;
    }

    SkillManager->OnSkillsInitialized.RemoveDynamic(this, &USkillBarWidget::OnPlayerSkillsInitialized);
    SkillManager->OnSkillCooldownStarted.RemoveDynamic(this, &USkillBarWidget::OnSkillCooldownStarted);
    SkillManager->OnSkillReady.RemoveDynamic(this, &USkillBarWidget::OnSkillReady);
    SkillManager->OnSkillSlotChanged.RemoveDynamic(this, &USkillBarWidget::OnSkillSlotChanged);

    bEventsSubscribed = false;
}

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