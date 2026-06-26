#include "UI/BestiaryEntryWidget.h"
#include "UI/BestiaryEntryWidget.h"
#include "UI/BestiaryTierRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UBestiaryEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UBestiaryEntryWidget::HandleCloseClicked);

    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.AddDynamic(this, &UBestiaryEntryWidget::HandleLocaleChanged);
        }
    }

    // Center standalone widgets on screen (embedded widgets have a parent and are positioned by layout)
    if (!GetParent())
    {
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
    }
}

void UBestiaryEntryWidget::NativeDestruct()
{
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.RemoveDynamic(this, &UBestiaryEntryWidget::HandleLocaleChanged);
        }
    }
    Super::NativeDestruct();
}

void UBestiaryEntryWidget::HandleCloseClicked()
{
    OnCloseRequested.Broadcast();
}

void UBestiaryEntryWidget::DisplayEntry(const FBestiaryEntryStruct& Entry)
{
    CurrentEntry = Entry;

    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    // Mob name
    if (Mob_Name_Text)
    {
        FText MobName = Loc ? Loc->GetMobDisplayName(Entry.mobSlug) : FText::FromString(Entry.mobSlug);
        Mob_Name_Text->SetText(MobName);
    }

    // Mob description
    if (Mob_Description_Text)
    {
        FText MobDesc = Loc ? Loc->GetMobDescription(Entry.mobSlug) : FText::GetEmpty();
        Mob_Description_Text->SetText(MobDesc);
    }

    // Mob icon from DT_MobDefinitions
    if (Mob_Icon)
    {
        Mob_Icon->SetVisibility(ESlateVisibility::Collapsed);
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UDataTable* DT = GI->GetMobDefinitionTable())
            {
                if (const FMobDefinition* Row = DT->FindRow<FMobDefinition>(FName(*Entry.mobSlug), TEXT("")))
                {
                    if (!Row->Visual.Icon.IsNull())
                    {
                        TWeakObjectPtr<UBestiaryEntryWidget> WeakThis = this;
                        TSoftObjectPtr<UTexture2D> IconPtr = Row->Visual.Icon;
                        if (UTexture2D* Already = IconPtr.Get())
                        {
                            Mob_Icon->SetBrushFromTexture(Already);
                            Mob_Icon->SetVisibility(ESlateVisibility::Visible);
                        }
                        else
                        {
                            UAssetManager::GetStreamableManager().RequestAsyncLoad(
                                IconPtr.ToSoftObjectPath(),
                                FStreamableDelegate::CreateLambda([WeakThis, IconPtr]()
                                {
                                    if (WeakThis.IsValid())
                                    {
                                        if (UTexture2D* Loaded = IconPtr.Get())
                                        {
                                            WeakThis->Mob_Icon->SetBrushFromTexture(Loaded);
                                            WeakThis->Mob_Icon->SetVisibility(ESlateVisibility::Visible);
                                        }
                                    }
                                })
                            );
                        }
                    }
                }
            }
        }
    }

    // Kill count
    if (Kill_Count_Text)
    {
        Kill_Count_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Kills: %d"), Entry.killCount)));
    }

    // Rebuild tiers
    if (Tiers_Box)
    {
        Tiers_Box->ClearChildren();
        for (const FBestiaryTierStruct& Tier : Entry.tiers)
        {
            BuildTierRow(Tier);
        }
    }
}

void UBestiaryEntryWidget::ClearEntry()
{
    CurrentEntry = FBestiaryEntryStruct();

    if (Mob_Name_Text)          Mob_Name_Text->SetText(FText::GetEmpty());
    if (Mob_Description_Text)   Mob_Description_Text->SetText(FText::GetEmpty());
    if (Mob_Icon)               Mob_Icon->SetVisibility(ESlateVisibility::Collapsed);
    if (Kill_Count_Text)        Kill_Count_Text->SetText(FText::GetEmpty());
    if (Tiers_Box)              Tiers_Box->ClearChildren();
}

void UBestiaryEntryWidget::BuildTierRow(const FBestiaryTierStruct& Tier)
{
    if (!Tiers_Box) return;

    if (!TierRowClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("BestiaryEntryWidget: BuildTierRow skipped - TierRowClass is not set"));
        return;
    }

    UBestiaryTierRowWidget* Row = CreateWidget<UBestiaryTierRowWidget>(GetOwningPlayer(), TierRowClass);
    if (!Row) return;

    Row->Setup(Tier);
    Tiers_Box->AddChild(Row);
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

FReply UBestiaryEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Embedded widgets are dragged by their parent — do not handle independently
        if (GetParent()) return FReply::Unhandled();

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

FReply UBestiaryEntryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (GetParent()) return FReply::Unhandled();
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            return FReply::Handled().ReleaseMouseCapture();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply UBestiaryEntryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (GetParent()) return FReply::Unhandled();
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void UBestiaryEntryWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VP = FVector2D(W, H) / Scale;
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(600, 600);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, VP.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, VP.Y - Size.Y));
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

void UBestiaryEntryWidget::HandleLocaleChanged(const FString& NewLocale)
{
    if (!CurrentEntry.mobSlug.IsEmpty())
    {
        DisplayEntry(CurrentEntry);
    }
}

