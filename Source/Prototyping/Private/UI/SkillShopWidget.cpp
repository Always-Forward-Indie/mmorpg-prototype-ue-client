#include "UI/SkillShopWidget.h"
#include "UI/SkillShopRowWidget.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "MyGameInstance.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// ---------------------------------------------------------------------------
// USkillShopRowBinding
// ---------------------------------------------------------------------------

void USkillShopRowBinding::Setup(USkillShopWidget* InWidget, const FString& InSkillSlug)
{
    Widget    = InWidget;
    SkillSlug = InSkillSlug;
}

void USkillShopRowBinding::HandleClicked()
{
    if (Widget) Widget->DispatchLearnSkill(SkillSlug);
}

// ---------------------------------------------------------------------------
// USkillShopWidget
// ---------------------------------------------------------------------------

void USkillShopWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &USkillShopWidget::HandleCloseButtonClicked);

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

void USkillShopWidget::BindToSkillShopManager(USkillShopManager* InManager)
{
    if (!InManager) return;

    if (SkillShopManager)
    {
        SkillShopManager->OnSkillShopOpenedDelegate.RemoveDynamic(this, &USkillShopWidget::HandleSkillShopOpened);
        SkillShopManager->OnSkillLearnedDelegate   .RemoveDynamic(this, &USkillShopWidget::HandleSkillLearned);
        SkillShopManager->OnSkillLearnFailedDelegate.RemoveDynamic(this, &USkillShopWidget::HandleSkillLearnFailed);
    }

    SkillShopManager = InManager;
    SkillShopManager->OnSkillShopOpenedDelegate .AddDynamic(this, &USkillShopWidget::HandleSkillShopOpened);
    SkillShopManager->OnSkillLearnedDelegate    .AddDynamic(this, &USkillShopWidget::HandleSkillLearned);
    SkillShopManager->OnSkillLearnFailedDelegate.AddDynamic(this, &USkillShopWidget::HandleSkillLearnFailed);
}

void USkillShopWidget::OpenShop()
{
    ClearStatus();
    RefreshDisplay();
    SetVisibility(ESlateVisibility::Visible);
    OnSkillShopVisibilityChanged.Broadcast(true);
}

void USkillShopWidget::CloseShop()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnSkillShopVisibilityChanged.Broadcast(false);
}

void USkillShopWidget::RefreshDisplay()
{
    UpdateHeaderDisplay();
    PopulateSkillRows();
}

void USkillShopWidget::DispatchLearnSkill(const FString& SkillSlug)
{
    if (!SkillShopManager) return;
    SkillShopManager->RequestLearnSkill(ActiveNpcId, SkillSlug);
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void USkillShopWidget::HandleSkillShopOpened(const FSkillShopData& ShopData)
{
    CachedShop  = ShopData;
    ActiveNpcId = ShopData.npcId;   // must be set BEFORE RequestLearnSkill is called
    UE_LOG(LogTemp, Log, TEXT("[SkillShopWidget] Shop opened: npcId=%d npcSlug='%s'"),
        ActiveNpcId, *ShopData.npcSlug);
    OpenShop();
}

void USkillShopWidget::HandleSkillLearned(const FLearnSkillResultData& Result)
{
    ShowStatus(FString::Printf(TEXT("Learned: %s"), *Result.skillName));

    // Refresh the shop display from the updated manager state
    if (SkillShopManager)
    {
        CachedShop = SkillShopManager->GetCurrentShop();
    }
    RefreshDisplay();
}

void USkillShopWidget::HandleSkillLearnFailed(const FString& SkillSlug, const FString& Reason)
{
    FString Msg = FString::Printf(TEXT("Cannot learn %s: %s"), *SkillSlug, *Reason);
    // Map reason codes to human-friendly text
    if (Reason == TEXT("insufficient_sp"))   Msg = FString::Printf(TEXT("Not enough Skill Points to learn %s"), *SkillSlug);
    else if (Reason == TEXT("insufficient_gold"))   Msg = FString::Printf(TEXT("Not enough gold to learn %s"), *SkillSlug);
    else if (Reason == TEXT("missing_skill_book"))  Msg = FString::Printf(TEXT("Need a skill book to learn %s"), *SkillSlug);
    else if (Reason == TEXT("already_learned"))     Msg = FString::Printf(TEXT("%s is already learned"), *SkillSlug);
    else if (Reason == TEXT("insufficient_level"))  Msg = FString::Printf(TEXT("Level too low to learn %s"), *SkillSlug);
    else if (Reason == TEXT("missing_prerequisite"))Msg = FString::Printf(TEXT("Missing prerequisite for %s"), *SkillSlug);
    else if (Reason == TEXT("out_of_range"))        Msg = TEXT("Move closer to the trainer");
    ShowStatus(Msg);
}

void USkillShopWidget::HandleCloseButtonClicked()
{
    CloseShop();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void USkillShopWidget::UpdateHeaderDisplay()
{
    if (NPC_Name_Text)
    {
        FString Name = CachedShop.npcSlug.IsEmpty()
            ? FString::Printf(TEXT("Trainer #%d"), CachedShop.npcId)
            : CachedShop.npcSlug;
        NPC_Name_Text->SetText(FText::FromString(Name));
    }

    if (Player_SP_Text)
        Player_SP_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Free SP: %d"), CachedShop.freeSkillPoints)));

    if (Player_Gold_Text)
        Player_Gold_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Gold: %d"), CachedShop.goldBalance)));
}

void USkillShopWidget::PopulateSkillRows()
{
    if (!Skill_List_Box || !SkillRowClass) return;

    Skill_List_Box->ClearChildren();
    RowBindings.Reset();

    // Resolve SkillDefinitionRepository for icon lookup
    USkillDefinitionRepository* SkillRepo = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        SkillRepo = GI->GetSkillDefinitionRepository();
    }

    for (const FSkillShopSkillData& Skill : CachedShop.skills)
    {
        USkillShopRowWidget* Row = CreateWidget<USkillShopRowWidget>(GetOwningPlayer(), SkillRowClass);
        if (!Row) continue;

        // Let the row widget populate its own bound fields
        Row->PopulateFromSkillData(Skill);

        // --- Icon: load from SkillDefinitionRepository and inject into the typed field ---
        if (Row->Skill_Icon_Image && SkillRepo)
        {
            FSkillDefinitionData Def = SkillRepo->GetDefinition(Skill.skillSlug);
            if (Def.skillIcon.IsValid())
            {
                if (UTexture2D* IconTex = Def.skillIcon.LoadSynchronous())
                {
                    Row->Skill_Icon_Image->SetBrushFromTexture(IconTex);
                }
            }
        }

        // --- Wire Learn button click to C++ dispatcher ---
        if (Row->Learn_Button && !Skill.isLearned)
        {
            USkillShopRowBinding* Binding = NewObject<USkillShopRowBinding>(this);
            Binding->Setup(this, Skill.skillSlug);
            RowBindings.Add(Binding);
            Row->Learn_Button->OnClicked.AddDynamic(Binding, &USkillShopRowBinding::HandleClicked);
        }

        Skill_List_Box->AddChild(Row);
    }
}

// SetRowState is superseded by USkillShopRowWidget::PopulateFromSkillData.
// Kept as a no-op so existing callers compile without changes.
void USkillShopWidget::SetRowState(UUserWidget* /*Row*/, const FSkillShopSkillData& /*Skill*/)
{
}

void USkillShopWidget::ShowStatus(const FString& Msg)
{
    if (Status_Text)
    {
        Status_Text->SetText(FText::FromString(Msg));
        Status_Text->SetVisibility(ESlateVisibility::Visible);
    }
}

void USkillShopWidget::ClearStatus()
{
    if (Status_Text)
        Status_Text->SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Drag support (mirrors RepairShopWidget pattern)
// ---------------------------------------------------------------------------

FReply USkillShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USkillShopWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USkillShopWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USkillShopWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    CurrentViewportPosition = ScreenCursorPos / Scale - DragOffset;
    SetPositionInViewport(CurrentViewportPosition, false);
}
