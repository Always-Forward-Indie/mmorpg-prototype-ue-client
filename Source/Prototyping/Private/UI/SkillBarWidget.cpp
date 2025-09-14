#include "UI/SkillBarWidget.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Framework/Application/SlateApplication.h"

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

    // Set up default hotkeys (1-9, 0)
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

    // Create skill slots if not already created
    if (SkillSlots.Num() == 0)
    {
        CreateSkillSlots(DefaultNumSlots);
    }

    // Subscribe to skill manager events if available
    if (SkillManager)
    {
        SubscribeToSkillManagerEvents();
    }
}

void USkillBarWidget::NativeDestruct()
{
    UnsubscribeFromSkillManagerEvents();
    Super::NativeDestruct();
}

FReply USkillBarWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Handle hotkey presses
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

    // Create initial skill slots
    CreateSkillSlots(DefaultNumSlots);
}

void USkillBarWidget::CreateSkillSlots(int32 NumSlots)
{
    if (!SkillSlotsContainer && !SkillGridContainer)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillBarWidget: No container widget found for skill slots"));
        return;
    }

    // Clear existing slots
    SkillSlots.Empty();
    
    if (SkillSlotsContainer)
    {
        SkillSlotsContainer->ClearChildren();
    }
    
    if (SkillGridContainer)
    {
        SkillGridContainer->ClearChildren();
    }

    // Create new slots
    for (int32 i = 0; i < NumSlots; ++i)
    {
        USkillSlotWidget* SlotWidget = CreateSkillSlotWidget(i);
        if (SlotWidget)
        {
            SkillSlots.Add(SlotWidget);

            // Add to appropriate container
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

    // Get current slot data from skill manager
    FSkillSlotData SlotData = SkillManager->GetSkillSlot(SlotIndex);
    SlotWidget->SetSlotData(SlotData);
}

void USkillBarWidget::AssignSkillToSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& Hotkey)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL ASSIGNMENT DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("AssignSkillToSlot called:"));
    UE_LOG(LogTemp, Warning, TEXT("  - SlotIndex: %d"), SlotIndex);
    UE_LOG(LogTemp, Warning, TEXT("  - SkillSlug: %s"), *SkillSlug);
    UE_LOG(LogTemp, Warning, TEXT("  - Hotkey: %s"), *Hotkey.ToString());
    UE_LOG(LogTemp, Warning, TEXT("  - Total Slots: %d"), SkillSlots.Num());

    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid parameters:"));
        UE_LOG(LogTemp, Error, TEXT("  - SkillManager: %s"), SkillManager ? TEXT("✅") : TEXT("❌"));
        UE_LOG(LogTemp, Error, TEXT("  - SlotIndex valid: %s"), 
            (SlotIndex >= 0 && SlotIndex < SkillSlots.Num()) ? TEXT("✅") : TEXT("❌"));
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL ASSIGNMENT DEBUG END (INVALID PARAMS) ==="));
        return;
    }

    if (!SkillManager->HasSkill(SkillSlug))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Skill not found in SkillManager: %s"), *SkillSlug);
        
        // Debug: List all available skills
        TArray<FPlayerSkillData> AllSkills = SkillManager->GetAllPlayerSkills();
        UE_LOG(LogTemp, Warning, TEXT("Available skills (%d):"), AllSkills.Num());
        for (int32 i = 0; i < FMath::Min(AllSkills.Num(), 5); ++i) // Show first 5
        {
            UE_LOG(LogTemp, Warning, TEXT("  - [%d] %s"), i, *AllSkills[i].networkData.skillSlug);
        }
        if (AllSkills.Num() > 5)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - ... and %d more"), AllSkills.Num() - 5);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL ASSIGNMENT DEBUG END (SKILL NOT FOUND) ==="));
        return;
    }

    // Get current slot state before assignment
    FSkillSlotData CurrentSlotData = SkillManager->GetSkillSlot(SlotIndex);
    UE_LOG(LogTemp, Warning, TEXT("Current slot state:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Assigned: %s"), CurrentSlotData.bIsAssigned ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Warning, TEXT("  - Current skill: %s"), *CurrentSlotData.skillSlug);
    UE_LOG(LogTemp, Warning, TEXT("  - Current hotkey: %s"), *CurrentSlotData.boundKey.ToString());

    // Assign skill through skill manager
    UE_LOG(LogTemp, Warning, TEXT("Calling SkillManager->SetSkillSlot..."));
    SkillManager->SetSkillSlot(SlotIndex, SkillSlug, Hotkey);

    // Verify assignment
    FSkillSlotData NewSlotData = SkillManager->GetSkillSlot(SlotIndex);
    bool bAssignmentSuccess = (NewSlotData.skillSlug == SkillSlug && NewSlotData.bIsAssigned);
    
    UE_LOG(LogTemp, Warning, TEXT("Assignment result:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Success: %s"), bAssignmentSuccess ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Warning, TEXT("  - New skill: %s"), *NewSlotData.skillSlug);
    UE_LOG(LogTemp, Warning, TEXT("  - New hotkey: %s"), *NewSlotData.boundKey.ToString());
    UE_LOG(LogTemp, Warning, TEXT("  - Assigned flag: %s"), NewSlotData.bIsAssigned ? TEXT("✅") : TEXT("❌"));

    if (bAssignmentSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Skill assigned successfully: %s → Slot %d"), *SkillSlug, SlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Skill assignment failed!"));
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL ASSIGNMENT DEBUG END ==="));
}

void USkillBarWidget::ClearSlot(int32 SlotIndex)
{
    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        return;
    }

    // Clear slot through skill manager
    SkillManager->SetSkillSlot(SlotIndex, "", FKey());
}

void USkillBarWidget::CastSkillFromSlot(int32 SlotIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL CAST DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("CastSkillFromSlot called for slot %d"), SlotIndex);

    if (!SkillManager || SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid parameters:"));
        UE_LOG(LogTemp, Error, TEXT("  - SkillManager: %s"), SkillManager ? TEXT("✅") : TEXT("❌"));
        UE_LOG(LogTemp, Error, TEXT("  - SlotIndex: %d (valid: %s)"), SlotIndex,
            (SlotIndex >= 0 && SlotIndex < SkillSlots.Num()) ? TEXT("✅") : TEXT("❌"));
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL CAST DEBUG END (INVALID PARAMS) ==="));
        return;
    }

    USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
    if (!SlotWidget || !SlotWidget->IsSlotAssigned())
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Slot %d is empty or invalid:"), SlotIndex);
        UE_LOG(LogTemp, Warning, TEXT("  - Widget exists: %s"), SlotWidget ? TEXT("✅") : TEXT("❌"));
        if (SlotWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - Is assigned: %s"), SlotWidget->IsSlotAssigned() ? TEXT("✅") : TEXT("❌"));
            UE_LOG(LogTemp, Warning, TEXT("  - Skill slug: %s"), *SlotWidget->GetAssignedSkillSlug());
        }
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL CAST DEBUG END (EMPTY SLOT) ==="));
        return;
    }

    FString SkillSlug = SlotWidget->GetAssignedSkillSlug();
    UE_LOG(LogTemp, Warning, TEXT("Attempting to cast skill: %s"), *SkillSlug);
    
    // Check if skill can be cast
    bool bCanCast = SkillManager->CanCastSkill(SkillSlug);
    UE_LOG(LogTemp, Warning, TEXT("Skill cast check:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Can cast: %s"), bCanCast ? TEXT("✅") : TEXT("❌"));
    
    if (!bCanCast)
    {
        // Debug why skill cannot be cast
        UE_LOG(LogTemp, Warning, TEXT("Skill cast blocked - checking reasons:"));
        
        if (SkillManager->IsSkillOnCooldown(SkillSlug))
        {
            float Remaining = SkillManager->GetSkillCooldownRemaining(SkillSlug);
            UE_LOG(LogTemp, Warning, TEXT("  - ❌ On cooldown: %.1f seconds remaining"), Remaining);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  - ✅ Not on cooldown"));
        }
        
        // Add more checks here if needed (mana, requirements, etc.)
        
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL CAST DEBUG END (CANNOT CAST) ==="));
        return;
    }

    // Log current target
    UE_LOG(LogTemp, Warning, TEXT("Cast parameters:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Target ID: %d"), CurrentTargetId);
    UE_LOG(LogTemp, Warning, TEXT("  - Target Type: %d"), (int32)CurrentTargetType);

    // Cast skill through skill manager
    UE_LOG(LogTemp, Warning, TEXT("Calling SkillManager->TryCastSkill..."));
    bool bSuccess = SkillManager->TryCastSkill(SkillSlug, CurrentTargetId, CurrentTargetType);
    
    UE_LOG(LogTemp, Warning, TEXT("Cast result: %s"), bSuccess ? TEXT("✅ SUCCESS") : TEXT("❌ FAILED"));
    
    if (bSuccess)
    {
        OnSkillCast.Broadcast(SlotIndex, SkillSlug);
        UE_LOG(LogTemp, Warning, TEXT("✅ Skill cast successful: %s from slot %d"), *SkillSlug, SlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Skill cast failed: %s from slot %d"), *SkillSlug, SlotIndex);
        UE_LOG(LogTemp, Error, TEXT("  - Check SkillManager->TryCastSkill implementation for details"));
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL CAST DEBUG END ==="));
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
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Left M Button clicked slot %d"), SlotIndex);
    CastSkillFromSlot(SlotIndex);
}

void USkillBarWidget::OnSkillSlotRightClicked(int32 SlotIndex, const FString& SkillSlug)
{
    // Right click could open skill context menu or clear slot
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Right M clicked slot %d"), SlotIndex);
    // Could implement context menu here
}

void USkillBarWidget::OnSkillDroppedOnSlot(int32 SlotIndex, const FPlayerSkillData& SkillData, const FKey& Hotkey)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL DROP DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Skill drop detected"));
    UE_LOG(LogTemp, Warning, TEXT("  - Skill: %s"), *SkillData.networkData.skillSlug);
    UE_LOG(LogTemp, Warning, TEXT("  - Target Slot: %d"), SlotIndex);
    UE_LOG(LogTemp, Warning, TEXT("  - Hotkey: %s"), *Hotkey.ToString());
    
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillBarWidget: ❌ Cannot handle skill drop - SkillManager not available"));
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL DROP DEBUG END (FAILED) ==="));
        return;
    }

    // Принудительно сбрасываем состояние всех слотов для избежания застревания highlight
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        if (USkillSlotWidget* SlotWidget = SkillSlots[i])
        {
            SlotWidget->ForceResetDragState();
        }
    }

    // Log current slot state before assignment
    if (SlotIndex >= 0 && SlotIndex < SkillSlots.Num())
    {
        USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
        if (SlotWidget)
        {
            FString CurrentSkill = SlotWidget->GetAssignedSkillSlug();
            UE_LOG(LogTemp, Warning, TEXT("  - Slot %d current skill: %s"), SlotIndex, 
                CurrentSkill.IsEmpty() ? TEXT("(empty)") : *CurrentSkill);
        }
    }

    // Log skill manager state
    UE_LOG(LogTemp, Warning, TEXT("  - SkillManager available: ✅"));
    UE_LOG(LogTemp, Warning, TEXT("  - Skill exists in manager: %s"), 
        SkillManager->HasSkill(SkillData.networkData.skillSlug) ? TEXT("✅") : TEXT("❌"));

    // Assign the dropped skill to the slot
    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Attempting skill assignment..."));
    AssignSkillToSlot(SlotIndex, SkillData.networkData.skillSlug, Hotkey);
    
    // Log result
    if (SlotIndex >= 0 && SlotIndex < SkillSlots.Num())
    {
        USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
        if (SlotWidget)
        {
            FString NewSkill = SlotWidget->GetAssignedSkillSlug();
            bool bSuccess = (NewSkill == SkillData.networkData.skillSlug);
            UE_LOG(LogTemp, Warning, TEXT("  - Assignment result: %s"), bSuccess ? TEXT("✅ SUCCESS") : TEXT("❌ FAILED"));
            UE_LOG(LogTemp, Warning, TEXT("  - Slot %d new skill: %s"), SlotIndex, 
                NewSkill.IsEmpty() ? TEXT("(empty)") : *NewSkill);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL DROP DEBUG END ==="));
}

void USkillBarWidget::OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills)
{
    UE_LOG(LogTemp, Log, TEXT("SkillBarWidget: Player skills initialized, refreshing slots"));
    RefreshAllSlots();
}

void USkillBarWidget::OnSkillCooldownStarted(const FString& SkillSlug)
{
    // Find slots with this skill and update their cooldown display
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug)
        {
            RefreshSlot(i);
        }
    }
}

void USkillBarWidget::OnSkillReady(const FString& SkillSlug)
{
    // Find slots with this skill and update their state
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget && SlotWidget->GetAssignedSkillSlug() == SkillSlug)
        {
            RefreshSlot(i);
        }
    }
}

void USkillBarWidget::OnSkillSlotChanged(int32 SlotIndex, const FSkillSlotData& SlotData)
{
    RefreshSlot(SlotIndex);
}

void USkillBarWidget::SubscribeToSkillManagerEvents()
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Cannot subscribe - SkillManager is null"));
        return;
    }

    SkillManager->OnSkillsInitialized.AddDynamic(this, &USkillBarWidget::OnPlayerSkillsInitialized);
    SkillManager->OnSkillCooldownStarted.AddDynamic(this, &USkillBarWidget::OnSkillCooldownStarted);
    SkillManager->OnSkillReady.AddDynamic(this, &USkillBarWidget::OnSkillReady);
    SkillManager->OnSkillSlotChanged.AddDynamic(this, &USkillBarWidget::OnSkillSlotChanged);

    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Subscribed to SkillManager events"));
}

void USkillBarWidget::UnsubscribeFromSkillManagerEvents()
{
    if (!SkillManager) return;

    SkillManager->OnSkillsInitialized.RemoveDynamic(this, &USkillBarWidget::OnPlayerSkillsInitialized);
    SkillManager->OnSkillCooldownStarted.RemoveDynamic(this, &USkillBarWidget::OnSkillCooldownStarted);
    SkillManager->OnSkillReady.RemoveDynamic(this, &USkillBarWidget::OnSkillReady);
    SkillManager->OnSkillSlotChanged.RemoveDynamic(this, &USkillBarWidget::OnSkillSlotChanged);

    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget: Unsubscribed from SkillManager events"));
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
        
        // Bind events
        SlotWidget->OnSkillSlotClicked.AddDynamic(this, &USkillBarWidget::OnSkillSlotClicked);
        SlotWidget->OnSkillSlotRightClicked.AddDynamic(this, &USkillBarWidget::OnSkillSlotRightClicked);
        SlotWidget->OnSkillDroppedOnSlot.AddDynamic(this, &USkillBarWidget::OnSkillDroppedOnSlot);
    }

    return SlotWidget;
}

void USkillBarWidget::SetupDefaultHotkeys()
{
    if (!SkillManager)
    {
        return;
    }

    // Assign default hotkeys to slots
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

// =========================
// DEBUG HELPER METHODS
// =========================

void USkillBarWidget::DebugPrintSkillBarState()
{
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL BAR STATE DEBUG ==="));
    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget State:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Total Slots: %d"), SkillSlots.Num());
    UE_LOG(LogTemp, Warning, TEXT("  - SkillManager: %s"), SkillManager ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Warning, TEXT("  - GameInstance: %s"), GameInstance ? TEXT("✅") : TEXT("❌"));
    
    if (SkillManager)
    {
        TArray<FPlayerSkillData> AllSkills = SkillManager->GetAllPlayerSkills();
        UE_LOG(LogTemp, Warning, TEXT("  - Available Skills: %d"), AllSkills.Num());
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Slot States:"));
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        if (USkillSlotWidget* SlotWidget = SkillSlots[i])
        {
            FString SkillSlug = SlotWidget->GetAssignedSkillSlug();
            bool bAssigned = SlotWidget->IsSlotAssigned();
            bool bOnCooldown = SlotWidget->IsOnCooldown();
            
            UE_LOG(LogTemp, Warning, TEXT("  - Slot[%d]: %s | Skill: %s | Cooldown: %s"), 
                i, 
                bAssigned ? TEXT("✅") : TEXT("❌"),
                SkillSlug.IsEmpty() ? TEXT("(empty)") : *SkillSlug,
                bOnCooldown ? TEXT("⏰") : TEXT("✅"));
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL BAR STATE DEBUG END ==="));
}

void USkillBarWidget::DebugPrintAvailableSkills()
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("DebugPrintAvailableSkills: SkillManager not available"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== AVAILABLE SKILLS DEBUG ==="));
    TArray<FPlayerSkillData> AllSkills = SkillManager->GetAllPlayerSkills();
    UE_LOG(LogTemp, Warning, TEXT("Total Available Skills: %d"), AllSkills.Num());
    
    for (int32 i = 0; i < AllSkills.Num(); ++i)
    {
        const FPlayerSkillData& Skill = AllSkills[i];
        UE_LOG(LogTemp, Warning, TEXT("  [%d] %s (Level: %d, CD: %dms)"), 
            i, 
            *Skill.networkData.skillSlug,
            Skill.networkData.skillLevel,
            Skill.networkData.cooldownMs);
    }
    UE_LOG(LogTemp, Warning, TEXT("=== AVAILABLE SKILLS DEBUG END ==="));
}

void USkillBarWidget::DebugTestSkillAssignment(int32 SlotIndex, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Warning, TEXT("=== DEBUG TEST SKILL ASSIGNMENT ==="));
    UE_LOG(LogTemp, Warning, TEXT("Testing assignment: %s → Slot %d"), *SkillSlug, SlotIndex);
    
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SkillManager not available"));
        return;
    }
    
    // Test if skill exists
    bool bSkillExists = SkillManager->HasSkill(SkillSlug);
    UE_LOG(LogTemp, Warning, TEXT("Skill exists: %s"), bSkillExists ? TEXT("✅") : TEXT("❌"));
    
    if (!bSkillExists)
    {
        UE_LOG(LogTemp, Warning, TEXT("Available skills:"));
        DebugPrintAvailableSkills();
        return;
    }
    
    // Test assignment
    AssignSkillToSlot(SlotIndex, SkillSlug, EKeys::F1); // Use F1 as test hotkey
    
    // Verify result
    if (SlotIndex >= 0 && SlotIndex < SkillSlots.Num())
    {
        USkillSlotWidget* SlotWidget = SkillSlots[SlotIndex];
        if (SlotWidget)
        {
            bool bSuccess = (SlotWidget->GetAssignedSkillSlug() == SkillSlug);
            UE_LOG(LogTemp, Warning, TEXT("Test result: %s"), bSuccess ? TEXT("✅ SUCCESS") : TEXT("❌ FAILED"));
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== DEBUG TEST END ==="));
}

void USkillBarWidget::DebugCheckDragDropSetup()
{
    UE_LOG(LogTemp, Warning, TEXT("=== DRAG DROP SETUP DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget Drag-Drop Setup Check:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Total Slots: %d"), SkillSlots.Num());
    UE_LOG(LogTemp, Warning, TEXT("  - SkillSlotWidgetClass: %s"), 
        SkillSlotWidgetClass ? *SkillSlotWidgetClass->GetName() : TEXT("NULL"));
    
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - Slot[%d]:"), i);
            UE_LOG(LogTemp, Warning, TEXT("    - Widget exists: ✅"));
            UE_LOG(LogTemp, Warning, TEXT("    - Visibility: %s"), 
                SlotWidget->GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") : 
                SlotWidget->GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") : 
                SlotWidget->GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") : 
                SlotWidget->GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") : 
                SlotWidget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
            
            UE_LOG(LogTemp, Warning, TEXT("    - SlotIndex: %d"), SlotWidget->GetSlotIndex());
            UE_LOG(LogTemp, Warning, TEXT("    - Assigned Skill: %s"), 
                SlotWidget->GetAssignedSkillSlug().IsEmpty() ? TEXT("(empty)") : *SlotWidget->GetAssignedSkillSlug());
            
            // Check if slot has required widgets
            if (UButton* SkillButton = Cast<UButton>(SlotWidget->GetWidgetFromName(TEXT("SkillButton"))))
            {
                UE_LOG(LogTemp, Warning, TEXT("    - SkillButton: ✅"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("    - SkillButton: ❌ NOT FOUND"));
            }
            
            if (UImage* DropHighlightBorder = Cast<UImage>(SlotWidget->GetWidgetFromName(TEXT("DropHighlightBorder"))))
            {
                UE_LOG(LogTemp, Warning, TEXT("    - DropHighlightBorder: ✅"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("    - DropHighlightBorder: ⚠️ NOT FOUND (optional)"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("  - Slot[%d]: ❌ NULL WIDGET"), i);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== DRAG DROP SETUP DEBUG END ==="));
}

void USkillBarWidget::DebugTestSlotHitTesting()
{
    UE_LOG(LogTemp, Warning, TEXT("=== SLOT HIT TESTING DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Testing slot hit testing capabilities:"));
    
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - Slot[%d]:"), i);
            
            // Check widget configuration
            bool bCanReceiveEvents = SlotWidget->GetVisibility() == ESlateVisibility::Visible || 
                                   SlotWidget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
            UE_LOG(LogTemp, Warning, TEXT("    - Can receive events: %s"), bCanReceiveEvents ? TEXT("✅") : TEXT("❌"));
            
            // Check geometry
            FGeometry SlotGeometry = SlotWidget->GetCachedGeometry();
            FVector2D SlotSize = SlotGeometry.GetLocalSize();
            UE_LOG(LogTemp, Warning, TEXT("    - Size: %.1f x %.1f"), SlotSize.X, SlotSize.Y);
            
            // Check if size is valid
            bool bValidSize = SlotSize.X > 0 && SlotSize.Y > 0;
            UE_LOG(LogTemp, Warning, TEXT("    - Valid size: %s"), bValidSize ? TEXT("✅") : TEXT("❌"));
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== SLOT HIT TESTING DEBUG END ==="));
}

void USkillBarWidget::DebugSimulateSkillDrop(int32 SlotIndex, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Warning, TEXT("=== DEBUG SIMULATE SKILL DROP START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Simulating skill drop: %s → Slot %d"), *SkillSlug, SlotIndex);
    
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SkillManager not available"));
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG SIMULATE SKILL DROP END (FAILED) ==="));
        return;
    }
    
    if (SlotIndex < 0 || SlotIndex >= SkillSlots.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid slot index: %d (valid range: 0-%d)"), SlotIndex, SkillSlots.Num()-1);
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG SIMULATE SKILL DROP END (FAILED) ==="));
        return;
    }
    
    if (!SkillManager->HasSkill(SkillSlug))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Skill not found in SkillManager: %s"), *SkillSlug);
        UE_LOG(LogTemp, Warning, TEXT("=== DEBUG SIMULATE SKILL DROP END (FAILED) ==="));
        return;
    }
    
    // Get skill data
    FPlayerSkillData SkillData = SkillManager->GetSkillData(SkillSlug);
    
    // Simulate the drop by calling the slot's drop handler directly
    USkillSlotWidget* TargetSlot = SkillSlots[SlotIndex];
    if (TargetSlot)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Calling OnSkillDroppedOnSlot directly..."));
        OnSkillDroppedOnSlot(SlotIndex, SkillData, EKeys::F1);
        UE_LOG(LogTemp, Warning, TEXT("✅ Simulated drop completed"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Target slot widget is NULL"));
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== DEBUG SIMULATE SKILL DROP END ==="));
}

void USkillBarWidget::DebugCheckWidgetOverlap()
{
    UE_LOG(LogTemp, Warning, TEXT("=== WIDGET OVERLAP DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Checking for potential widget overlap issues..."));
    
    // Check SkillBarWidget's own visibility and layout
    UE_LOG(LogTemp, Warning, TEXT("SkillBarWidget state:"));
    UE_LOG(LogTemp, Warning, TEXT("  - Visibility: %s"), 
        GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") :
        GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") :
        GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") :
        GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") :
        GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
    
    // Check root widget type
    UWidget* RootWidget = GetRootWidget();
    if (RootWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("  - Root widget type: %s"), *RootWidget->GetClass()->GetName());
        UE_LOG(LogTemp, Warning, TEXT("  - Root widget visibility: %s"), 
            RootWidget->GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") :
            RootWidget->GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") :
            RootWidget->GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") :
            RootWidget->GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") :
            RootWidget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
        
        // Check if root is Canvas Panel (potential issue)
        if (RootWidget->GetClass()->GetName().Contains(TEXT("Canvas")))
        {
            UE_LOG(LogTemp, Warning, TEXT("  - ⚠️ ROOT IS CANVAS PANEL - this can cause drag-drop issues!"));
            UE_LOG(LogTemp, Warning, TEXT("  - Canvas panels can interfere with drag events"));
            UE_LOG(LogTemp, Warning, TEXT("  - Consider using Border or UserWidget as root instead"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("  - ❌ Root widget is NULL"));
    }
    
    // Check containers
    if (SkillSlotsContainer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotsContainer state:"));
        UE_LOG(LogTemp, Warning, TEXT("  - Type: %s"), *SkillSlotsContainer->GetClass()->GetName());
        UE_LOG(LogTemp, Warning, TEXT("  - Visibility: %s"), 
            SkillSlotsContainer->GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") :
            SkillSlotsContainer->GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") :
            SkillSlotsContainer->GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") :
            SkillSlotsContainer->GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") :
            SkillSlotsContainer->GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
        UE_LOG(LogTemp, Warning, TEXT("  - Children count: %d"), SkillSlotsContainer->GetChildrenCount());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotsContainer: ❌ NULL"));
    }
    
    if (SkillGridContainer)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillGridContainer state:"));
        UE_LOG(LogTemp, Warning, TEXT("  - Type: %s"), *SkillGridContainer->GetClass()->GetName());
        UE_LOG(LogTemp, Warning, TEXT("  - Visibility: %s"), 
            SkillGridContainer->GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") :
            SkillGridContainer->GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") :
            SkillGridContainer->GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") :
            SkillGridContainer->GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") :
            SkillGridContainer->GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
        UE_LOG(LogTemp, Warning, TEXT("  - Children count: %d"), SkillGridContainer->GetChildrenCount());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillGridContainer: ⚠️ NULL (optional)"));
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== WIDGET OVERLAP DEBUG END ==="));
}

void USkillBarWidget::DebugTestSlotHitTestingDetailed()
{
    UE_LOG(LogTemp, Warning, TEXT("=== DETAILED SLOT HIT TESTING DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Testing detailed hit-testing for all slots..."));
    
    for (int32 i = 0; i < SkillSlots.Num(); ++i)
    {
        USkillSlotWidget* SlotWidget = SkillSlots[i];
        if (SlotWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("--- Slot[%d] Detailed Analysis ---"), i);
            
            // Check widget hierarchy
            UWidget* Parent = SlotWidget->GetParent();
            if (Parent)
            {
                UE_LOG(LogTemp, Warning, TEXT("  - Parent type: %s"), *Parent->GetClass()->GetName());
                UE_LOG(LogTemp, Warning, TEXT("  - Parent visibility: %s"), 
                    Parent->GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") :
                    Parent->GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") :
                    Parent->GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") :
                    Parent->GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") :
                    Parent->GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
            }
            
            // Check slot's root widget
            UWidget* SlotRoot = SlotWidget->GetRootWidget();
            if (SlotRoot)
            {
                UE_LOG(LogTemp, Warning, TEXT("  - Slot root type: %s"), *SlotRoot->GetClass()->GetName());
                if (SlotRoot->GetClass()->GetName().Contains(TEXT("Canvas")))
                {
                    UE_LOG(LogTemp, Warning, TEXT("  - ⚠️ SLOT ROOT IS CANVAS PANEL - potential drag-drop issue!"));
                }
            }
            
            // Check geometry
            FGeometry SlotGeometry = SlotWidget->GetCachedGeometry();
            FVector2D SlotSize = SlotGeometry.GetLocalSize();
            FVector2D SlotPosition = SlotGeometry.GetAbsolutePosition();
            
            UE_LOG(LogTemp, Warning, TEXT("  - Size: %.1f x %.1f"), SlotSize.X, SlotSize.Y);
            UE_LOG(LogTemp, Warning, TEXT("  - Position: %.1f, %.1f"), SlotPosition.X, SlotPosition.Y);
            
            // Check for overlapping with siblings
            for (int32 j = 0; j < SkillSlots.Num(); ++j)
            {
                if (i != j && SkillSlots[j])
                {
                    FGeometry OtherGeometry = SkillSlots[j]->GetCachedGeometry();
                    FVector2D OtherPosition = OtherGeometry.GetAbsolutePosition();
                    FVector2D OtherSize = OtherGeometry.GetLocalSize();
                    
                    // Simple overlap check
                    bool bOverlapping = !(SlotPosition.X >= OtherPosition.X + OtherSize.X ||
                                        OtherPosition.X >= SlotPosition.X + SlotSize.X ||
                                        SlotPosition.Y >= OtherPosition.Y + OtherSize.Y ||
                                        OtherPosition.Y >= SlotPosition.Y + SlotSize.Y);
                    
                    if (bOverlapping)
                    {
                        UE_LOG(LogTemp, Error, TEXT("  - ❌ OVERLAPPING with Slot[%d]!"), j);
                    }
                }
            }
            
            // Check child widgets that might block events
            if (UButton* SkillButton = Cast<UButton>(SlotWidget->GetWidgetFromName(TEXT("SkillButton"))))
            {
                UE_LOG(LogTemp, Warning, TEXT("  - SkillButton visibility: %s"), 
                    SkillButton->GetVisibility() == ESlateVisibility::Visible ? TEXT("Visible") :
                    SkillButton->GetVisibility() == ESlateVisibility::Hidden ? TEXT("Hidden") :
                    SkillButton->GetVisibility() == ESlateVisibility::Collapsed ? TEXT("Collapsed") :
                    SkillButton->GetVisibility() == ESlateVisibility::HitTestInvisible ? TEXT("HitTestInvisible") :
                    SkillButton->GetVisibility() == ESlateVisibility::SelfHitTestInvisible ? TEXT("SelfHitTestInvisible") : TEXT("Unknown"));
                
                if (SkillButton->GetVisibility() == ESlateVisibility::Visible)
                {
                    UE_LOG(LogTemp, Warning, TEXT("  - ⚠️ SkillButton is Visible - might block drag events"));
                    UE_LOG(LogTemp, Warning, TEXT("  - Consider setting to HitTestInvisible during drag"));
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("=== DETAILED SLOT HIT TESTING DEBUG END ==="));
}

void USkillBarWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    
    // Установка размера корневого Border виджета
    if (UBorder* RootBorder = Cast<UBorder>(GetRootWidget()))
    {
        // Устанавливаем фиксированный размер
        RootBorder->SetDesiredSizeScale(FVector2D(1.0f, 1.0f));
        
        // Или через слот, если Border находится в другом контейнере
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RootBorder->Slot))
        {
            CanvasSlot->SetSize(FVector2D(800.0f, 80.0f));
            CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f)); // Bottom Center
            CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
            CanvasSlot->SetPosition(FVector2D(0.0f, -100.0f)); // 100px от низа
        }
    }
}