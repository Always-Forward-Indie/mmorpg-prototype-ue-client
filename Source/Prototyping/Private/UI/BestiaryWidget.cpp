#include "UI/BestiaryWidget.h"
#include "UI/BestiaryWidget.h"
#include "UI/BestiaryEntryWidget.h"
#include "UI/BestiaryMobRowWidget.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UBestiaryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UBestiaryWidget::HandleCloseClicked);

    if (Entry_Panel)
        Entry_Panel->OnCloseRequested.AddDynamic(this, &UBestiaryWidget::HandleEntryPanelCloseRequested);

    // Resolve character ID from game instance
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        CurrentCharacterId = GI->GetCurrentCharacterID();

    // Start collapsed
    SetVisibility(ESlateVisibility::Collapsed);

    // Centre on screen
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D((W - Size.X) * 0.5f, (H - Size.Y) * 0.5f);
        SetPositionInViewport(CurrentViewportPosition, false);
    }
}

void UBestiaryWidget::BindToBestiaryHandler(UBestiaryNetworkHandler* InHandler)
{
    if (BestiaryHandler)
    {
        BestiaryHandler->OnBestiaryOverviewReceived.RemoveDynamic(this, &UBestiaryWidget::HandleBestiaryOverviewReceived);
        BestiaryHandler->OnBestiaryEntryReceived   .RemoveDynamic(this, &UBestiaryWidget::HandleBestiaryEntryReceived);
        BestiaryHandler->OnBestiaryTierUnlocked    .RemoveDynamic(this, &UBestiaryWidget::HandleBestiaryTierUnlocked);
        BestiaryHandler->OnBestiaryKillCountUpdated.RemoveDynamic(this, &UBestiaryWidget::HandleBestiaryKillCountUpdated);
    }

    BestiaryHandler = InHandler;

    if (BestiaryHandler)
    {
        BestiaryHandler->OnBestiaryOverviewReceived.AddDynamic(this, &UBestiaryWidget::HandleBestiaryOverviewReceived);
        BestiaryHandler->OnBestiaryEntryReceived   .AddDynamic(this, &UBestiaryWidget::HandleBestiaryEntryReceived);
        BestiaryHandler->OnBestiaryTierUnlocked    .AddDynamic(this, &UBestiaryWidget::HandleBestiaryTierUnlocked);
        BestiaryHandler->OnBestiaryKillCountUpdated.AddDynamic(this, &UBestiaryWidget::HandleBestiaryKillCountUpdated);

        // Replay any overview that arrived before this widget was bound
        if (BestiaryHandler->HasPendingOverview())
        {
            UE_LOG(LogTemp, Log, TEXT("BestiaryWidget: Replaying pending overview received before bind"));
            HandleBestiaryOverviewReceived(BestiaryHandler->ConsumePendingOverview());
        }
    }
}

void UBestiaryWidget::AddOrUpdateMobEntry(const FString& MobSlug, int32 KillCount)
{
    for (FBestiaryMobListEntry& Existing : CachedMobList)
    {
        if (Existing.MobSlug == MobSlug)
        {
            Existing.KillCount = KillCount;
            RebuildMobList();
            return;
        }
    }

    FBestiaryMobListEntry NewEntry;
    NewEntry.MobSlug   = MobSlug;
    NewEntry.KillCount = KillCount;
    CachedMobList.Add(NewEntry);

    RebuildMobList();
}

void UBestiaryWidget::OpenBestiary()
{
    RebuildMobList();
    SetVisibility(ESlateVisibility::Visible);
    OnBestiaryVisibilityChanged.Broadcast(true);
}

void UBestiaryWidget::CloseBestiary()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnBestiaryVisibilityChanged.Broadcast(false);

    if (Entry_Panel)
        Entry_Panel->SetVisibility(ESlateVisibility::Collapsed);
    if (StandaloneEntryWidget)
        StandaloneEntryWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UBestiaryWidget::ToggleBestiary()
{
    if (GetVisibility() == ESlateVisibility::Visible)
        CloseBestiary();
    else
        OpenBestiary();
}

void UBestiaryWidget::HandleCloseClicked()
{
    CloseBestiary();
}

void UBestiaryWidget::HandleEntryPanelCloseRequested()
{
    if (Entry_Panel)
        Entry_Panel->SetVisibility(ESlateVisibility::Collapsed);
    if (StandaloneEntryWidget)
        StandaloneEntryWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UBestiaryWidget::HandleMobRowSelected(const FString& MobSlug)
{
    RequestAndShowEntry(MobSlug);
}

void UBestiaryWidget::HandleBestiaryOverviewReceived(const TArray<FBestiaryOverviewEntryStruct>& Entries)
{
    for (const FBestiaryOverviewEntryStruct& OverviewEntry : Entries)
    {
        AddOrUpdateMobEntry(OverviewEntry.mobSlug, OverviewEntry.killCount);
    }
}

void UBestiaryWidget::HandleBestiaryKillCountUpdated(const FString& MobSlug, int32 KillCount)
{
    AddOrUpdateMobEntry(MobSlug, KillCount);
}

void UBestiaryWidget::HandleBestiaryEntryReceived(const FBestiaryEntryStruct& Entry)
{
    // Update kill count in cached list
    AddOrUpdateMobEntry(Entry.mobSlug, Entry.killCount);

    // Show entry only if this was the pending request
    if (Entry.mobSlug != PendingRequestMobSlug)
        return;
    PendingRequestMobSlug = TEXT("");

    UBestiaryEntryWidget* Target = Entry_Panel;
    if (!Target)
    {
        if (!StandaloneEntryWidget && MobEntryWidgetClass)
        {
            StandaloneEntryWidget = CreateWidget<UBestiaryEntryWidget>(GetOwningPlayer(), MobEntryWidgetClass);
            if (StandaloneEntryWidget)
            {
                StandaloneEntryWidget->AddToViewport(151);
                StandaloneEntryWidget->OnCloseRequested.AddDynamic(
                    this, &UBestiaryWidget::HandleEntryPanelCloseRequested);
            }
        }
        Target = StandaloneEntryWidget;
    }

    if (Target)
    {
        Target->DisplayEntry(Entry);
        Target->SetVisibility(ESlateVisibility::Visible);
    }
}

void UBestiaryWidget::HandleBestiaryTierUnlocked(const FString& MobSlug, int32 UnlockedTier,
                                                   const FString& /*CategorySlug*/)
{
    // Update the list entry kill count if present
    for (FBestiaryMobListEntry& E : CachedMobList)
    {
        if (E.MobSlug == MobSlug)
        {
            RebuildMobList();
            break;
        }
    }

    // If bestiary UI is open and showing this mob, refresh entry
    if (GetVisibility() != ESlateVisibility::Visible) return;
    if (!BestiaryHandler) return;

    UBestiaryEntryWidget* Target = Entry_Panel ? Entry_Panel : StandaloneEntryWidget;
    const bool bEntryVisible = Target && Target->GetVisibility() == ESlateVisibility::Visible;
    if (!bEntryVisible) return;

    if (BestiaryHandler->HasCachedEntry(MobSlug))
    {
        BestiaryHandler->InvalidateCacheEntry(MobSlug);
    }
    PendingRequestMobSlug = MobSlug;
    BestiaryHandler->RequestBestiaryEntry(CurrentCharacterId, MobSlug);
}

void UBestiaryWidget::RequestAndShowEntry(const FString& MobSlug)
{
    if (!BestiaryHandler) return;

    PendingRequestMobSlug = MobSlug;

    if (BestiaryHandler->HasCachedEntry(MobSlug))
    {
        HandleBestiaryEntryReceived(BestiaryHandler->GetCachedEntry(MobSlug));
        return;
    }

    BestiaryHandler->RequestBestiaryEntry(CurrentCharacterId, MobSlug);
}

void UBestiaryWidget::RebuildMobList()
{
    if (!Mob_List_Box)
    {
        UE_LOG(LogTemp, Warning, TEXT("BestiaryWidget: RebuildMobList skipped - Mob_List_Box is null"));
        return;
    }

    Mob_List_Box->ClearChildren();

    if (!MobRowClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("BestiaryWidget: RebuildMobList skipped - MobRowClass is not set"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("BestiaryWidget: Rebuilding mob list with %d entries"), CachedMobList.Num());

    for (const FBestiaryMobListEntry& Entry : CachedMobList)
    {
        UBestiaryMobRowWidget* Row = CreateWidget<UBestiaryMobRowWidget>(GetOwningPlayer(), MobRowClass);
        if (!Row) continue;

        Row->Setup(Entry.MobSlug, Entry.KillCount);
        Row->OnMobRowSelected.AddDynamic(this, &UBestiaryWidget::HandleMobRowSelected);

        Mob_List_Box->AddChild(Row);
    }
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

FReply UBestiaryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UBestiaryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UBestiaryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void UBestiaryWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VP = FVector2D(W, H) / Scale;
    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(600, 600);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, VP.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, VP.Y - Size.Y);
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
