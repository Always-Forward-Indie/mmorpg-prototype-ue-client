#include "UI/EmoteListWidget.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Data/EmoteDefinitionTable.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

// ---------------------------------------------------------------------------
// Construct / Destruct
// ---------------------------------------------------------------------------

void UEmoteListWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
    {
        Close_Button->OnClicked.AddDynamic(this, &UEmoteListWidget::HandleCloseClicked);
    }
    if (CategoryAll_Button)
    {
        CategoryAll_Button->OnClicked.AddDynamic(this, &UEmoteListWidget::HandleCategoryAll);
    }
    if (CategoryGeneral_Button)
    {
        CategoryGeneral_Button->OnClicked.AddDynamic(this, &UEmoteListWidget::HandleCategoryGeneral);
    }
    if (CategorySocial_Button)
    {
        CategorySocial_Button->OnClicked.AddDynamic(this, &UEmoteListWidget::HandleCategorySocial);
    }
    if (CategoryDance_Button)
    {
        CategoryDance_Button->OnClicked.AddDynamic(this, &UEmoteListWidget::HandleCategoryDance);
    }

    // Center on screen
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

void UEmoteListWidget::NativeDestruct()
{
    if (Manager)
    {
        Manager->OnPlayerEmotesLoaded.RemoveDynamic(this, &UEmoteListWidget::HandlePlayerEmotesLoaded);
    }
    Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UEmoteListWidget::BindToManagers(UEmoteManager* InManager, UEmoteNetworkHandler* InHandler, int32 InCharacterId)
{
    if (Manager)
    {
        Manager->OnPlayerEmotesLoaded.RemoveDynamic(this, &UEmoteListWidget::HandlePlayerEmotesLoaded);
    }

    Manager          = InManager;
    NetworkHandler   = InHandler;
    LocalCharacterId = InCharacterId;

    if (Manager)
    {
        Manager->OnPlayerEmotesLoaded.AddDynamic(this, &UEmoteListWidget::HandlePlayerEmotesLoaded);

        // If data is already cached (e.g. widget opened after login), build the grid immediately
        const FPlayerEmotesState& Cached = Manager->GetPlayerEmotesState();
        if (Cached.characterId > 0)
        {
            RebuildGrid(ActiveCategory);
        }
    }
}

void UEmoteListWidget::OpenEmoteList()
{
    RebuildGrid(ActiveCategory);
    SetVisibility(ESlateVisibility::Visible);
    bIsVisible = true;
    OnEmoteListVisibilityChanged.Broadcast();
}

void UEmoteListWidget::CloseEmoteList()
{
    SetVisibility(ESlateVisibility::Collapsed);
    bIsVisible = false;
    OnEmoteListVisibilityChanged.Broadcast();
}

void UEmoteListWidget::ToggleEmoteList()
{
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        CloseEmoteList();
    }
    else
    {
        OpenEmoteList();
    }
}

// ---------------------------------------------------------------------------
// Internal event handlers
// ---------------------------------------------------------------------------

void UEmoteListWidget::HandlePlayerEmotesLoaded(const FPlayerEmotesState& State)
{
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RebuildGrid(ActiveCategory);
    }
}

void UEmoteListWidget::HandleCloseClicked()
{
    CloseEmoteList();
}

void UEmoteListWidget::HandleCategoryAll()     { SetCategoryFilter(TEXT("")); }
void UEmoteListWidget::HandleCategoryGeneral() { SetCategoryFilter(TEXT("general")); }
void UEmoteListWidget::HandleCategorySocial()  { SetCategoryFilter(TEXT("social")); }
void UEmoteListWidget::HandleCategoryDance()   { SetCategoryFilter(TEXT("dance")); }

void UEmoteListWidget::SetCategoryFilter(const FString& NewFilter)
{
    ActiveCategory = NewFilter;

    if (CategoryLabel_Text)
    {
        FString Label = NewFilter.IsEmpty() ? TEXT("All") : NewFilter;
        Label[0] = FChar::ToUpper(Label[0]);
        CategoryLabel_Text->SetText(FText::FromString(Label));
    }

    RebuildGrid(ActiveCategory);
}

void UEmoteListWidget::HandleEmoteItemClicked(const FEmoteDefinitionData& EmoteDef)
{
    if (NetworkHandler && !EmoteDef.slug.IsEmpty())
    {
        NetworkHandler->RequestUseEmote(EmoteDef.slug);
    }
}

// ---------------------------------------------------------------------------
// Grid rebuild
// ---------------------------------------------------------------------------

void UEmoteListWidget::RebuildGrid(const FString& CategoryFilter)
{
    if (!Emotes_WrapBox || !EmoteItemWidgetClass || !Manager) return;

    Emotes_WrapBox->ClearChildren();

    TArray<FEmoteDefinitionData> Emotes = Manager->GetPlayerEmotesByCategory(CategoryFilter);

    for (const FEmoteDefinitionData& Def : Emotes)
    {
        UEmoteItemWidget* Item = CreateWidget<UEmoteItemWidget>(GetOwningPlayer(), EmoteItemWidgetClass);
        if (!Item) continue;

        Item->SetEmoteData(Def, EmoteDefinitionTable.Get());
        Item->OnEmoteItemClicked.AddDynamic(this, &UEmoteListWidget::HandleEmoteItemClicked);

        UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Emotes_WrapBox->AddChild(Item));
        if (WrapSlot)
        {
            WrapSlot->SetPadding(FMargin(2.f));
        }
    }
}

// ---------------------------------------------------------------------------
// Drag support — mirrors TitlesWidget pattern
// ---------------------------------------------------------------------------

FReply UEmoteListWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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
            {
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            }
            return FReply::Handled();
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UEmoteListWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UEmoteListWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UEmoteListWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale      = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VPSize = FVector2D(W, H) / Scale;
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400.f, 300.f);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, VPSize.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, VPSize.Y - Size.Y));
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
