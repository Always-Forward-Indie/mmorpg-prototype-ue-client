#include "UI/RepairShopWidget.h"
#include "Gameplay/Repair/RepairManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"

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
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D((W - Size.X) * 0.5f, (H - Size.Y) * 0.5f);
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

void URepairShopWidget::OpenShop()
{
    RefreshDisplay();
    SetVisibility(ESlateVisibility::Visible);
    OnRepairShopVisibilityChanged.Broadcast(true);
}

void URepairShopWidget::CloseShop()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnRepairShopVisibilityChanged.Broadcast(false);
}

void URepairShopWidget::RefreshDisplay()
{
    if (!Repair_Items_Box) return;

    Repair_Items_Box->ClearChildren();
    RowBindings.Reset();

    for (const FRepairShopItemData& Item : CachedShop.items)
    {
        if (!RepairRowClass) continue;

        UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), RepairRowClass);
        if (!Row) continue;

        if (UTextBlock* NameTxt = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Name_Text"))))
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
            NameTxt->SetText(ItemName);
        }

        if (UTextBlock* DurTxt = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Durability_Text"))))
            DurTxt->SetText(FText::FromString(
                FString::Printf(TEXT("%d / %d"), Item.durabilityCurrent, Item.durabilityMax)));

        if (UTextBlock* CostTxt = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Cost_Text"))))
            CostTxt->SetText(FText::FromString(
                FString::Printf(TEXT("%d g"), Item.repairCost)));

        if (UButton* Btn = Cast<UButton>(Row->GetWidgetFromName(TEXT("Row_Repair_Btn"))))
        {
            URepairItemRowBinding* Binding = NewObject<URepairItemRowBinding>(this);
            Binding->Setup(this, Item.inventoryItemId);
            RowBindings.Add(Binding);
            Btn->OnClicked.AddDynamic(Binding, &URepairItemRowBinding::HandleClicked);
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
    OpenShop();
}

void URepairShopWidget::HandleRepairItemResult(const FRepairItemResultData& Result)
{
    if (!Result.errorCode.IsEmpty())
    {
        ShowStatus(FString::Printf(TEXT("Repair failed: %s"), *Result.errorCode));
        return;
    }
    ShowStatus(FString::Printf(TEXT("Repaired  (-%d g)"), Result.goldSpent));

    // After repair durabilityCurrent == durabilityMax; update cached display
    for (FRepairShopItemData& Item : CachedShop.items)
    {
        if (Item.inventoryItemId == Result.inventoryItemId)
        {
            Item.durabilityCurrent = Result.durabilityMax;
            break;
        }
    }
    RefreshDisplay();
}

void URepairShopWidget::HandleRepairAllResult(const FRepairAllResultData& Result)
{
    if (!Result.errorCode.IsEmpty())
    {
        ShowStatus(FString::Printf(TEXT("Repair all failed: %s"), *Result.errorCode));
        return;
    }
    ShowStatus(FString::Printf(TEXT("All repaired  (-%d g)"), Result.goldSpent));

    // Update cached durability for all repaired items
    for (const FRepairedItemEntry& Entry : Result.repairedItems)
    {
        for (FRepairShopItemData& Item : CachedShop.items)
        {
            if (Item.inventoryItemId == Entry.inventoryItemId)
            {
                Item.durabilityCurrent = Entry.durabilityMax;
                break;
            }
        }
    }
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
    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 500);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, VP.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, VP.Y - Size.Y);
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
