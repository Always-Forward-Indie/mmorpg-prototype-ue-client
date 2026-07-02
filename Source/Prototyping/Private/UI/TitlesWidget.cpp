#include "UI/TitlesWidget.h"
#include "UI/TitlesWidget.h"
#include "UI/TitleRowWidget.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UTitlesWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UTitlesWidget::HandleCloseButtonClicked);

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

    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            Loc->OnLocaleChanged.AddDynamic(this, &UTitlesWidget::HandleLocaleChanged);
        }
    }
}

void UTitlesWidget::NativeDestruct()
{
    if (Manager)
        Manager->OnTitlesUpdated.RemoveDynamic(this, &UTitlesWidget::HandleTitlesUpdated);

    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            Loc->OnLocaleChanged.RemoveDynamic(this, &UTitlesWidget::HandleLocaleChanged);
        }
    }

    Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UTitlesWidget::BindToManagers(UTitleManager* InManager, UTitleNetworkHandler* InHandler, int32 InCharacterId)
{
    if (Manager)
        Manager->OnTitlesUpdated.RemoveDynamic(this, &UTitlesWidget::HandleTitlesUpdated);

    Manager     = InManager;
    Handler     = InHandler;
    CharacterId = InCharacterId;

    if (Manager)
    {
        Manager->OnTitlesUpdated.AddDynamic(this, &UTitlesWidget::HandleTitlesUpdated);
        const FPlayerTitlesState& Cached = Manager->GetCachedState();
        if (Cached.characterId > 0)
            RefreshAll(Cached);
    }
}

void UTitlesWidget::OpenTitles()
{
    // If we have no data yet, request it from the server
    if (CachedState.characterId == 0 && Handler && CharacterId > 0)
        Handler->RequestGetTitles(CharacterId);

    RefreshAll(CachedState.characterId > 0 ? CachedState
        : (Manager ? Manager->GetCachedState() : CachedState));

    SetVisibility(ESlateVisibility::Visible);
    OnTitlesVisibilityChanged.Broadcast();
}

void UTitlesWidget::CloseTitles()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnTitlesVisibilityChanged.Broadcast();
}

void UTitlesWidget::ToggleTitles()
{
    if (GetVisibility() == ESlateVisibility::Visible)
        CloseTitles();
    else
        OpenTitles();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void UTitlesWidget::HandleCloseButtonClicked()
{
    CloseTitles();
}

void UTitlesWidget::HandleRowEquipRequested(const FString& TitleSlug)
{
    if (Handler && CharacterId > 0)
        Handler->RequestEquipTitle(CharacterId, TitleSlug);
}

void UTitlesWidget::HandleTitlesUpdated(const FPlayerTitlesState& State)
{
    CachedState = State;
    if (GetVisibility() == ESlateVisibility::Visible)
        RefreshAll(State);
}

void UTitlesWidget::HandleLocaleChanged(const FString& NewLocale)
{
    if (CachedState.characterId > 0 && GetVisibility() == ESlateVisibility::Visible)
        RefreshAll(CachedState);
}

// ---------------------------------------------------------------------------
// Refresh logic
// ---------------------------------------------------------------------------

void UTitlesWidget::RefreshAll(const FPlayerTitlesState& State)
{
    // Resolve localization fallback helper
    auto ResolveName = [this](const FString& Slug, const FString& DisplayName) -> FText
    {
        if (!Slug.IsEmpty())
        {
            if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
            {
                if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                {
                    const FText LocName = Loc->GetTitleDisplayName(Slug);
                    if (!LocName.IsEmpty()) return LocName;
                }
            }
        }
        if (!DisplayName.IsEmpty()) return FText::FromString(DisplayName);
        return FText::FromString(Slug);
    };

    // Equipped title header
    if (EquippedTitle_Text)
    {
        if (State.equippedTitleSlug.IsEmpty())
            EquippedTitle_Text->SetText(FText::FromString(TEXT("Equipped: (none)")));
        else
            EquippedTitle_Text->SetText(FText::Format(
                FText::FromString(TEXT("Equipped: {0}")),
                ResolveName(State.equippedTitleSlug, State.equippedTitle.displayName)));
    }

    if (!Titles_ScrollBox) return;
    Titles_ScrollBox->ClearChildren();

    for (FTitleEntry Entry : State.earnedTitles)
    {
        const FText ResolvedName = ResolveName(Entry.slug, Entry.displayName);
        AddTitleRow(Entry, Entry.slug == State.equippedTitleSlug, ResolvedName);
    }
}

void UTitlesWidget::AddTitleRow(const FTitleEntry& Entry, bool bIsEquipped, const FText& ResolvedName)
{
    if (!TitleRowClass || !Titles_ScrollBox) return;

    UTitleRowWidget* Row = CreateWidget<UTitleRowWidget>(GetOwningPlayer(), TitleRowClass);
    if (!Row) return;

    Row->Populate(Entry, bIsEquipped, ResolvedName);
    Row->OnEquipRequested.AddDynamic(this, &UTitlesWidget::HandleRowEquipRequested);

    Titles_ScrollBox->AddChild(Row);
}


// Drag support
// ---------------------------------------------------------------------------

FReply UTitlesWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldDrag = false;
        if (DragHandle)
        {
            const FGeometry DragGeo  = DragHandle->GetCachedGeometry();
            const FVector2D Local    = DragGeo.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D DragSize = DragGeo.GetLocalSize();
            bShouldDrag = (Local.X >= 0.f && Local.X <= DragSize.X &&
                           Local.Y >= 0.f && Local.Y <= DragSize.Y);
        }
        else
        {
            bShouldDrag = true;
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
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UTitlesWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UTitlesWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UTitlesWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale       = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VPSize  = FVector2D(W, H) / Scale;
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400.f, 300.f);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, VPSize.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, VPSize.Y - Size.Y));
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
