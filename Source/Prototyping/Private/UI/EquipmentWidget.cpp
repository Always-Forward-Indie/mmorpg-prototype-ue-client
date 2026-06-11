#include "UI/EquipmentWidget.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

namespace
{
    static const TArray<FString>& GetAllSlotSlugs()
    {
        static const TArray<FString> Slugs = {
            TEXT("head"), TEXT("chest"), TEXT("legs"),      TEXT("feet"),
            TEXT("hands"), TEXT("waist"), TEXT("necklace"),
            TEXT("ring_1"), TEXT("ring_2"),
            TEXT("main_hand"), TEXT("off_hand"), TEXT("cloak")
        };
        return Slugs;
    }
}

FString UEquipmentWidget::GetSlotDisplayName(const FString& Slug)
{
    static const TMap<FString, FString> Names = {
        { TEXT("head"),      TEXT("Head") },
        { TEXT("chest"),     TEXT("Chest") },
        { TEXT("legs"),      TEXT("Legs") },
        { TEXT("feet"),      TEXT("Feet") },
        { TEXT("hands"),     TEXT("Hands") },
        { TEXT("waist"),     TEXT("Waist") },
        { TEXT("necklace"),  TEXT("Necklace") },
        { TEXT("ring_1"),    TEXT("Ring 1") },
        { TEXT("ring_2"),    TEXT("Ring 2") },
        { TEXT("main_hand"), TEXT("Main Hand") },
        { TEXT("off_hand"),  TEXT("Off Hand") },
        { TEXT("cloak"),     TEXT("Cloak") },
    };
    const FString* Found = Names.Find(Slug);
    return Found ? *Found : Slug;
}

void UEquipmentWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UEquipmentWidget::HandleCloseButtonClicked);

    InitializeSlots();

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

void UEquipmentWidget::InitializeSlots()
{
    TMap<FString, UEquipmentSlotWidget*> SlotMap = {
        { TEXT("head"),      Slot_Head      },
        { TEXT("chest"),     Slot_Chest     },
        { TEXT("legs"),      Slot_Legs      },
        { TEXT("feet"),      Slot_Feet      },
        { TEXT("hands"),     Slot_Hands     },
        { TEXT("waist"),     Slot_Waist     },
        { TEXT("necklace"),  Slot_Necklace  },
        { TEXT("ring_1"),    Slot_Ring1     },
        { TEXT("ring_2"),    Slot_Ring2     },
        { TEXT("main_hand"), Slot_MainHand  },
        { TEXT("off_hand"),  Slot_OffHand   },
        { TEXT("cloak"),     Slot_Cloak     },
    };

    for (const FString& Slug : GetAllSlotSlugs())
    {
        if (UEquipmentSlotWidget** WidgetPtr = SlotMap.Find(Slug))
        {
            UEquipmentSlotWidget* W = *WidgetPtr;
            if (!W) continue;
            W->InitializeSlot(Slug, GetSlotDisplayName(Slug));
            BindSlotDelegates(W);
        }
    }
}

void UEquipmentWidget::BindSlotDelegates(UEquipmentSlotWidget* InSlot)
{
    if (!InSlot) return;
    InSlot->OnEquipSlotClicked     .AddDynamic(this, &UEquipmentWidget::HandleSlotClicked);
    InSlot->OnEquipSlotRightClicked.AddDynamic(this, &UEquipmentWidget::HandleSlotRightClicked);
    InSlot->OnEquipSlotHovered     .AddDynamic(this, &UEquipmentWidget::HandleSlotHovered);
}

void UEquipmentWidget::BindToEquipmentManager(UEquipmentManager* InEquipmentManager, UInventoryManager* InInventoryManager)
{
    if (!InEquipmentManager) return;

    if (EquipmentManager)
    {
        EquipmentManager->OnEquipmentStateChangedDelegate.RemoveDynamic(this, &UEquipmentWidget::HandleEquipmentStateChanged);
        EquipmentManager->OnEquipResultReceivedDelegate  .RemoveDynamic(this, &UEquipmentWidget::HandleEquipResultReceived);
    }

    EquipmentManager = InEquipmentManager;
    InventoryManager = InInventoryManager;

    EquipmentManager->OnEquipmentStateChangedDelegate.AddDynamic(this, &UEquipmentWidget::HandleEquipmentStateChanged);
    EquipmentManager->OnEquipResultReceivedDelegate  .AddDynamic(this, &UEquipmentWidget::HandleEquipResultReceived);

    RefreshEquipmentDisplay();
}

void UEquipmentWidget::RefreshEquipmentDisplay()
{
    if (!EquipmentManager) return;
    CachedState = EquipmentManager->GetEquipmentState();

    TMap<FString, UEquipmentSlotWidget*> SlotMap = {
        { TEXT("head"),      Slot_Head      },
        { TEXT("chest"),     Slot_Chest     },
        { TEXT("legs"),      Slot_Legs      },
        { TEXT("feet"),      Slot_Feet      },
        { TEXT("hands"),     Slot_Hands     },
        { TEXT("waist"),     Slot_Waist     },
        { TEXT("necklace"),  Slot_Necklace  },
        { TEXT("ring_1"),    Slot_Ring1     },
        { TEXT("ring_2"),    Slot_Ring2     },
        { TEXT("main_hand"), Slot_MainHand  },
        { TEXT("off_hand"),  Slot_OffHand   },
        { TEXT("cloak"),     Slot_Cloak     },
    };

    for (const FString& Slug : GetAllSlotSlugs())
    {
        UEquipmentSlotWidget** WidgetPtr = SlotMap.Find(Slug);
        if (!WidgetPtr || !(*WidgetPtr)) continue;

        const FEquipmentSlotData* SlotData = CachedState.slots.Find(Slug);
        if (SlotData)
            (*WidgetPtr)->SetSlotData(*SlotData);
        else
            (*WidgetPtr)->ClearSlot();
    }
}

void UEquipmentWidget::ToggleEquipment()
{
    const bool bVisible = GetVisibility() == ESlateVisibility::Visible;
    if (bVisible)
    {
        if (EquipTooltipWidget) EquipTooltipWidget->HideTooltip();
        SetVisibility(ESlateVisibility::Collapsed);
        OnEquipmentVisibilityChanged.Broadcast(false);
    }
    else
    {
        RefreshEquipmentDisplay();
        SetVisibility(ESlateVisibility::Visible);
        OnEquipmentVisibilityChanged.Broadcast(true);
    }
}

void UEquipmentWidget::HandleEquipmentStateChanged(const FEquipmentStateData& State)
{
    CachedState = State;
    RefreshEquipmentDisplay();
}

void UEquipmentWidget::HandleEquipResultReceived(const FEquipResultData& Result)
{
    if (!Result.errorCode.IsEmpty())
        UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: equip error - %s"), *Result.errorCode);
}

void UEquipmentWidget::HandleCloseButtonClicked()
{
    if (EquipTooltipWidget) EquipTooltipWidget->HideTooltip();
    SetVisibility(ESlateVisibility::Collapsed);
    OnEquipmentVisibilityChanged.Broadcast(false);
}

void UEquipmentWidget::HandleSlotClicked(const FString& SlotSlug)
{
    OnEquipSlotLeftClicked.Broadcast(SlotSlug);
}

void UEquipmentWidget::HandleSlotRightClicked(const FString& SlotSlug)
{
    const FEquipmentSlotData* SlotData = CachedState.slots.Find(SlotSlug);
    if (SlotData && SlotData->bIsOccupied)
        OnEquipSlotUnequipRequested.Broadcast(SlotSlug);
}

void UEquipmentWidget::HandleSlotHovered(const FString& SlotSlug, bool bHovered)
{
    if (!EquipTooltipWidget) return;
    if (!bHovered) { EquipTooltipWidget->HideTooltip(); return; }
    ShowTooltipForSlot(SlotSlug);
}

void UEquipmentWidget::ShowTooltipForSlot(const FString& SlotSlug)
{
    if (!EquipTooltipWidget) return;
    const FEquipmentSlotData* SlotData = CachedState.slots.Find(SlotSlug);
    if (!SlotData || !SlotData->bIsOccupied) return;
    FInventoryItemStruct Item = BuildTooltipItem(*SlotData);
    EquipTooltipWidget->SetItemData(Item);
    EquipTooltipWidget->ShowTooltip();
    EquipTooltipWidget->UpdateTooltipPosition(FVector2D::ZeroVector);
}

FInventoryItemStruct UEquipmentWidget::BuildTooltipItem(const FEquipmentSlotData& SlotData) const
{
    if (InventoryManager)
    {
        const FInventoryItemStruct Found = InventoryManager->GetItemById(SlotData.inventoryItemId);
        if (Found.id > 0) return Found;
    }
    FInventoryItemStruct Stub;
    Stub.id               = SlotData.inventoryItemId;
    Stub.itemId           = SlotData.itemId;
    Stub.slug             = SlotData.itemSlug;
    Stub.isDurable        = (SlotData.durabilityMax > 0);
    Stub.durabilityMax    = SlotData.durabilityMax;
    Stub.durabilityCurrent = SlotData.durabilityCurrent;
    Stub.isEquippable     = true;
    Stub.is_equipped      = true;
    return Stub;
}

FReply UEquipmentWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldDrag = false;
        if (DragHandle)
        {
            const FGeometry DragGeo  = DragHandle->GetCachedGeometry();
            const FVector2D Local    = DragGeo.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D DragSize = DragGeo.GetLocalSize();
            bShouldDrag = (Local.X >= 0 && Local.X <= DragSize.X && Local.Y >= 0 && Local.Y <= DragSize.Y);
        }
        else { bShouldDrag = true; }

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

FReply UEquipmentWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UEquipmentWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging) { UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition()); return FReply::Handled(); }
    return FReply::Unhandled();
}

void UEquipmentWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D ViewportSize = FVector2D(W, H) / Scale;
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 600);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, ViewportSize.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - Size.Y));
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
