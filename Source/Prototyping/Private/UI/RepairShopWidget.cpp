#include "UI/RepairShopWidget.h"
#include "UI/RepairShopRowWidget.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

// ---------------------------------------------------------------------------
// URepairItemRowBinding
// ---------------------------------------------------------------------------

void URepairItemRowBinding::Setup(URepairShopWidget* InWidget, int32 InInventoryItemId)
{
    Widget          = InWidget;
    InventoryItemId = InInventoryItemId;
}

void URepairItemRowBinding::HandleClicked()
{
    if (Widget) Widget->DispatchRepairItem(InventoryItemId);
}

// ---------------------------------------------------------------------------
// URepairShopWidget
// ---------------------------------------------------------------------------

void URepairShopWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &URepairShopWidget::HandleCloseButtonClicked);

    if (Repair_All_Btn)
        Repair_All_Btn->OnClicked.AddDynamic(this, &URepairShopWidget::HandleRepairAllClicked);

    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
        const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D(
            FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
            FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void URepairShopWidget::BindToRepairManager(URepairManager* InRepairManager)
{
    if (!InRepairManager) return;

    if (RepairManager)
    {
        RepairManager->OnRepairShopOpenedDelegate  .RemoveDynamic(this, &URepairShopWidget::HandleRepairShopOpened);
        RepairManager->OnRepairItemResultDelegate  .RemoveDynamic(this, &URepairShopWidget::HandleRepairItemResult);
        RepairManager->OnRepairAllResultDelegate   .RemoveDynamic(this, &URepairShopWidget::HandleRepairAllResult);
    }

    RepairManager = InRepairManager;
    RepairManager->OnRepairShopOpenedDelegate.AddDynamic(this, &URepairShopWidget::HandleRepairShopOpened);
    RepairManager->OnRepairItemResultDelegate.AddDynamic(this, &URepairShopWidget::HandleRepairItemResult);
    RepairManager->OnRepairAllResultDelegate .AddDynamic(this, &URepairShopWidget::HandleRepairAllResult);
}

void URepairShopWidget::BindToInventoryManager(UInventoryManager* InInventoryManager)
{
    if (InventoryManager)
        InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &URepairShopWidget::HandleInventoryUpdated);

    InventoryManager = InInventoryManager;

    if (InventoryManager)
        InventoryManager->OnInventoryUpdated.AddDynamic(this, &URepairShopWidget::HandleInventoryUpdated);
}

void URepairShopWidget::OpenShop()
{
    UpdateGoldText();
    RefreshDisplay();
    SetVisibility(ESlateVisibility::Visible);
    OnRepairShopVisibilityChanged.Broadcast(true);
}

void URepairShopWidget::CloseShop()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnRepairShopVisibilityChanged.Broadcast(false);

    // Notify NPC that this shop window closed; farewell plays if no other windows remain.
    if (CachedShop.npcId > 0)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UNPCManager* NPCMgr = GI->GetNPCManager())
            {
                if (ABasicNPC* NPC = NPCMgr->GetNPCById(CachedShop.npcId))
                {
                    NPC->NotifyWindowClosed();
                }
            }
        }
    }
}

void URepairShopWidget::RefreshDisplay()
{
    if (!Repair_Items_Box) return;

    Repair_Items_Box->ClearChildren();
    RowBindings.Reset();

    for (const FRepairShopItemData& Item : CachedShop.items)
    {
        if (!RepairRowClass) continue;

        URepairShopRowWidget* Row = CreateWidget<URepairShopRowWidget>(GetOwningPlayer(), RepairRowClass);
        if (!Row) continue;

        if (Row->Row_Name_Text)
        {
            FText ItemName = FText::FromString(Item.slug);
            if (!Item.slug.IsEmpty())
            {
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        const FText LocName = Loc->GetItemDisplayName(Item.slug);
                        if (!LocName.IsEmpty()) ItemName = LocName;
                    }
                }
            }
            Row->Row_Name_Text->SetText(ItemName);
        }

        if (Row->Row_Durability_Text)
            Row->Row_Durability_Text->SetText(FText::FromString(
                FString::Printf(TEXT("%d / %d"), Item.durabilityCurrent, Item.durabilityMax)));

        if (Row->Row_Cost_Text)
            Row->Row_Cost_Text->SetText(FText::FromString(
                FString::Printf(TEXT("%d g"), Item.repairCost)));

        if (Row->Row_Repair_Btn)
        {
            URepairItemRowBinding* Binding = NewObject<URepairItemRowBinding>(this);
            Binding->Setup(this, Item.inventoryItemId);
            RowBindings.Add(Binding);
            Row->Row_Repair_Btn->OnClicked.AddDynamic(Binding, &URepairItemRowBinding::HandleClicked);
        }

        Repair_Items_Box->AddChild(Row);
    }

    if (Total_Cost_Text)
    {
        Total_Cost_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Total: %d g"), CachedShop.totalRepairCost)));
    }

    if (Repair_All_Btn)
    {
        Repair_All_Btn->SetIsEnabled(CachedShop.items.Num() > 0);
    }

    UpdateGoldText();
}

void URepairShopWidget::DispatchRepairItem(int32 InventoryItemId)
{
    if (!RepairManager) return;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    RepairManager->RequestRepairItem(CharId, CachedShop.npcId, InventoryItemId, FVector::ZeroVector);
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void URepairShopWidget::HandleRepairShopOpened(const FRepairShopData& ShopData)
{
    CachedShop = ShopData;
    // Server includes goldBalance in all repairShop packets; prefer it over the
    // InventoryManager fallback when it is present.
    if (ShopData.goldBalance > 0 || CachedGoldBalance == 0)
        CachedGoldBalance = ShopData.goldBalance;

    // Notify NPC that this shop window opened so the farewell counter stays balanced.
    if (CachedShop.npcId > 0)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UNPCManager* NPCMgr = GI->GetNPCManager())
            {
                if (ABasicNPC* NPC = NPCMgr->GetNPCById(CachedShop.npcId))
                {
                    NPC->NotifyWindowOpened();
                }
            }
        }
    }

    OpenShop();
}

void URepairShopWidget::HandleRepairItemResult(const FRepairItemResultData& Result)
{
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FString::Printf(TEXT("Repair failed: %s"), *Friendly.ToString()));
		return;
	}
    ShowStatus(FString::Printf(TEXT("Repaired  (-%d g)"), Result.goldSpent));

    // Update gold balance immediately (server also sends a follow-up repairShop
    // packet, but we update now for visual responsiveness)
    if (Result.newGoldBalance > 0)
        CachedGoldBalance = Result.newGoldBalance;
    else
        CachedGoldBalance = FMath::Max(0, CachedGoldBalance - Result.goldSpent);

    // Remove the repaired item from the list (fully repaired items don't need repair)
    CachedShop.items.RemoveAll([&Result](const FRepairShopItemData& Item)
    {
        return Item.inventoryItemId == Result.inventoryItemId;
    });

    // Recalculate total repair cost from remaining items
    int32 NewTotal = 0;
    for (const FRepairShopItemData& Item : CachedShop.items)
        NewTotal += Item.repairCost;
    CachedShop.totalRepairCost = NewTotal;
    CachedShop.repairAllCost   = NewTotal;

    RefreshDisplay();
}

void URepairShopWidget::HandleRepairAllResult(const FRepairAllResultData& Result)
{
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FString::Printf(TEXT("Repair all failed: %s"), *Friendly.ToString()));
		return;
	}

    const int32 TotalSpent = Result.goldSpent > 0 ? Result.goldSpent : Result.totalGoldSpent;
    ShowStatus(FString::Printf(TEXT("All repaired  (-%d g)"), TotalSpent));

    // Update gold balance immediately
    if (Result.newGoldBalance > 0)
        CachedGoldBalance = Result.newGoldBalance;
    else
        CachedGoldBalance = FMath::Max(0, CachedGoldBalance - TotalSpent);

    // All items are now fully repaired — clear the list
    CachedShop.items.Empty();
    CachedShop.totalRepairCost = 0;
    CachedShop.repairAllCost   = 0;

    RefreshDisplay();
}

void URepairShopWidget::HandleRepairAllClicked()
{
    if (!RepairManager) return;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    RepairManager->RequestRepairAll(CharId, CachedShop.npcId, FVector::ZeroVector);
}

void URepairShopWidget::HandleCloseButtonClicked()
{
    CloseShop();
}

void URepairShopWidget::HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory)
{
    // Keep gold display up to date whenever the inventory changes.
    // This handles the initial population (before repairShop goldBalance arrives)
    // and any background gold changes.
    CachedGoldBalance = Inventory.gold;
    if (GetVisibility() == ESlateVisibility::Visible)
        UpdateGoldText();
}

void URepairShopWidget::UpdateGoldText()
{
    if (Player_Gold_Text)
        Player_Gold_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Gold: %d"), CachedGoldBalance)));
}

void URepairShopWidget::ShowStatus(const FString& Msg)
{
    if (Status_Text)
    {
        Status_Text->SetText(FText::FromString(Msg));
        Status_Text->SetVisibility(ESlateVisibility::Visible);
    }
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

FReply URepairShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldDrag = !DragHandle;
        if (DragHandle)
        {
            const FGeometry G = DragHandle->GetCachedGeometry();
            const FVector2D L = G.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D S = G.GetLocalSize();
            bShouldDrag = (L.X >= 0 && L.X <= S.X && L.Y >= 0 && L.Y <= S.Y);
        }
        if (bShouldDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            DragOffset = InMouseEvent.GetScreenSpacePosition() / Scale - CurrentViewportPosition;
            bDragging  = true;
            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

FReply URepairShopWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            return FReply::Handled().ReleaseMouseCapture();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply URepairShopWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging) { UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition()); return FReply::Handled(); }
    return FReply::Unhandled();
}

void URepairShopWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VP = FVector2D(W, H) / Scale;
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 500);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, VP.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, VP.Y - Size.Y));
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

FText URepairShopWidget::GetVendorErrorText(const FString& ErrorCode) const
{
    if (const UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (const ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            return Loc->GetVendorErrorText(ErrorCode);
        }
    }
    FString Friendly = ErrorCode.Replace(TEXT("_"), TEXT(" "));
    if (!Friendly.IsEmpty())
        Friendly = Friendly.Left(1).ToUpper() + Friendly.Mid(1).ToLower();
    return FText::FromString(Friendly);
}
