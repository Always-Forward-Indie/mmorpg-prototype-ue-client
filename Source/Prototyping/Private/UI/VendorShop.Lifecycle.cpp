#include "UI/VendorShopWidget.h"
#include "Prototyping.h"
#include "UI/VendorShopWidget.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "UI/VendorTooltipWidget.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WrapBox.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UVendorShopWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleCloseButtonClicked);
    if (Tab_Buy_Button)
        Tab_Buy_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleTabBuyClicked);
    if (Tab_Sell_Button)
        Tab_Sell_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleTabSellClicked);
    if (Buy_Confirm_Button)
        Buy_Confirm_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleConfirmBuyCart);
    else
        UE_LOG(LogGameplay, Warning, TEXT("VendorShopWidget: Buy_Confirm_Button is not bound in Blueprint - buy cart cannot be confirmed"));
    if (Sell_Confirm_Button)
        Sell_Confirm_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleConfirmSellCart);
    else
        UE_LOG(LogGameplay, Warning, TEXT("VendorShopWidget: Sell_Confirm_Button is not bound in Blueprint - sell cart cannot be confirmed"));
    if (Buy_Clear_Button)
        Buy_Clear_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleClearBuyCart);
    if (Sell_Clear_Button)
        Sell_Clear_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleClearSellCart);

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

    // Create the tooltip widget and put it on top of everything
    if (VendorTooltipWidgetClass)
    {
        VendorTooltipWidget = CreateWidget<UVendorTooltipWidget>(GetOwningPlayer(), VendorTooltipWidgetClass);
        if (VendorTooltipWidget)
            VendorTooltipWidget->AddToViewport(1000);
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.AddDynamic(this, &UVendorShopWidget::HandleLocaleChanged);
        }
    }
}

void UVendorShopWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.RemoveDynamic(this, &UVendorShopWidget::HandleLocaleChanged);
        }
    }

    if (ActivePopup)
    {
        ActivePopup->ClosePopup();
        ActivePopup->RemoveFromParent();
        ActivePopup = nullptr;
    }
    if (VendorTooltipWidget)
    {
        VendorTooltipWidget->RemoveFromParent();
        VendorTooltipWidget = nullptr;
    }
    Super::NativeDestruct();
}





void UVendorShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!VendorTooltipWidget) return;
    const ESlateVisibility Vis = VendorTooltipWidget->GetVisibility();
    if (Vis != ESlateVisibility::HitTestInvisible && Vis != ESlateVisibility::SelfHitTestInvisible) return;

    VendorTooltipWidget->UpdateTooltipPosition();
}

void UVendorShopWidget::BindToManagers(UVendorManager* InVendorManager, UInventoryManager* InInventoryManager)
{
    if (!InVendorManager) return;

    // Unbind old
    if (VendorManager)
    {
        VendorManager->OnVendorShopOpenedDelegate   .RemoveDynamic(this, &UVendorShopWidget::HandleVendorShopOpened);
        VendorManager->OnBuyItemResultDelegate      .RemoveDynamic(this, &UVendorShopWidget::HandleBuyItemResult);
        VendorManager->OnSellItemResultDelegate     .RemoveDynamic(this, &UVendorShopWidget::HandleSellItemResult);
        VendorManager->OnBuyItemBatchResultDelegate .RemoveDynamic(this, &UVendorShopWidget::HandleBuyItemBatchResult);
        VendorManager->OnSellItemBatchResultDelegate.RemoveDynamic(this, &UVendorShopWidget::HandleSellItemBatchResult);
    }
    if (InventoryManager)
        InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &UVendorShopWidget::HandleInventoryUpdated);

    VendorManager    = InVendorManager;
    InventoryManager = InInventoryManager;

    VendorManager->OnVendorShopOpenedDelegate   .AddDynamic(this, &UVendorShopWidget::HandleVendorShopOpened);
    VendorManager->OnBuyItemResultDelegate      .AddDynamic(this, &UVendorShopWidget::HandleBuyItemResult);
    VendorManager->OnSellItemResultDelegate     .AddDynamic(this, &UVendorShopWidget::HandleSellItemResult);
    VendorManager->OnBuyItemBatchResultDelegate .AddDynamic(this, &UVendorShopWidget::HandleBuyItemBatchResult);
    VendorManager->OnSellItemBatchResultDelegate.AddDynamic(this, &UVendorShopWidget::HandleSellItemBatchResult);

    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.AddDynamic(this, &UVendorShopWidget::HandleInventoryUpdated);
        CachedInventory = InventoryManager->GetInventory();
    }
}

