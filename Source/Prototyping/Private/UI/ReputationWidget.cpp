#include "UI/ReputationWidget.h"
#include "Gameplay/Player/ReputationManager.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/UserWidget.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UReputationWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UReputationWidget::HandleCloseButtonClicked);

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

void UReputationWidget::NativeDestruct()
{
    if (Manager)
    {
        Manager->OnReputationsLoaded.RemoveDynamic(this, &UReputationWidget::HandleReputationsLoaded);
        Manager->OnReputationUpdated.RemoveDynamic(this, &UReputationWidget::HandleReputationUpdated);
    }

    Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UReputationWidget::BindToManager(UReputationManager* InManager)
{
    if (Manager)
    {
        Manager->OnReputationsLoaded.RemoveDynamic(this, &UReputationWidget::HandleReputationsLoaded);
        Manager->OnReputationUpdated.RemoveDynamic(this, &UReputationWidget::HandleReputationUpdated);
    }

    Manager = InManager;

    if (Manager)
    {
        Manager->OnReputationsLoaded.AddDynamic(this, &UReputationWidget::HandleReputationsLoaded);
        Manager->OnReputationUpdated.AddDynamic(this, &UReputationWidget::HandleReputationUpdated);

        // Populate immediately if we already have data
        if (Manager->GetAllReputations().Num() > 0)
            RefreshAll();
    }
}

void UReputationWidget::OpenReputation()
{
    RefreshAll();
    SetVisibility(ESlateVisibility::Visible);
    OnReputationVisibilityChanged.Broadcast();
}

void UReputationWidget::CloseReputation()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnReputationVisibilityChanged.Broadcast();
}

void UReputationWidget::ToggleReputation()
{
    if (GetVisibility() == ESlateVisibility::Visible)
        CloseReputation();
    else
        OpenReputation();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void UReputationWidget::HandleCloseButtonClicked()
{
    CloseReputation();
}

void UReputationWidget::HandleReputationsLoaded(const FPlayerReputationsState& /*State*/)
{
    if (GetVisibility() == ESlateVisibility::Visible)
        RefreshAll();
}

void UReputationWidget::HandleReputationUpdated(const FReputationUpdateData& /*Update*/)
{
    if (GetVisibility() == ESlateVisibility::Visible)
        RefreshAll();
}

// ---------------------------------------------------------------------------
// Refresh logic
// ---------------------------------------------------------------------------

void UReputationWidget::RefreshAll()
{
    if (!Reputation_ScrollBox || !Manager) return;
    Reputation_ScrollBox->ClearChildren();

    const TArray<FReputationEntry> Entries = Manager->GetAllReputations();
    for (const FReputationEntry& Entry : Entries)
    {
        AddReputationRow(Entry);
    }

    if (Entries.Num() == 0)
    {
        UTextBlock* TB = NewObject<UTextBlock>(this);
        TB->SetText(FText::FromString(TEXT("No reputation data yet.")));
        Reputation_ScrollBox->AddChild(TB);
    }
}

void UReputationWidget::AddReputationRow(const FReputationEntry& Entry)
{
    if (ReputationRowClass && Reputation_ScrollBox)
    {
        UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), ReputationRowClass);
        if (Row)
        {
            if (UTextBlock* FactionTB = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Faction_Text"))))
                FactionTB->SetText(FText::FromString(Entry.factionSlug));

            if (UTextBlock* ValueTB = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Value_Text"))))
                ValueTB->SetText(FText::FromString(FString::Printf(TEXT("%d"), Entry.value)));

            if (UTextBlock* TierTB = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Tier_Text"))))
                TierTB->SetText(FText::FromString(Entry.tier));

            Reputation_ScrollBox->AddChild(Row);
            return;
        }
    }

    // Fallback: plain TextBlock
    if (Reputation_ScrollBox)
    {
        UTextBlock* TB = NewObject<UTextBlock>(this);
        TB->SetText(FText::FromString(FString::Printf(TEXT("%s  %d  [%s]"),
            *Entry.factionSlug, Entry.value, *Entry.tier)));
        Reputation_ScrollBox->AddChild(TB);
    }
}

// ---------------------------------------------------------------------------
// Drag support (identical to TitlesWidget pattern)
// ---------------------------------------------------------------------------

FReply UReputationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UReputationWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UReputationWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UReputationWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D ViewportSize = FVector2D(W, H) / Scale;
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 300);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, ViewportSize.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - Size.Y));
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
