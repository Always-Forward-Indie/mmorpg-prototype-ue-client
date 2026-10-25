#include "UI/SkillSlotWidget.h"
#include "UI/SkillSlotWidget.h"
#include "Prototyping.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/SkillDragDropOperation.h"
#include "UI/SkillBarWidget.h"
#include "Blueprint/DragDropOperation.h"

USkillSlotWidget::USkillSlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SkillManager = nullptr;
    SlotIndex = -1;
    AssignedSkillSlug = "";
    bIsOnCooldown = false;
    bIsEnabled = true;
    bIsHighlighted = false;
    bIsDropHighlighted = false;
    bMousePressed = false;
    bIsDragging = false;
    CooldownRemainingTime = 0.0f;
    CooldownTotalTime = 0.0f;
}

void USkillSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Ensure this UserWidget itself is Visible so it participates in hit-testing.
    // This is what allows Slate to route DragOver/Drop events to us.
    SetVisibility(ESlateVisibility::Visible);

    if (SkillButton)
    {
        // Button handles clicks via our NativeOnMouseButtonDown/Up — we keep it
        // HitTestInvisible so it does NOT intercept the mouse hit-test chain.
        // This is critical: UE5 routes NativeOnDrop to the widget that "owns" the
        // last successful NativeOnDragOver. If SkillButton is Visible/Enabled it
        // steals the hit, UserWidget never gets DragOver, and NativeOnDrop never
        // fires. With HitTestInvisible the button is skipped for hit-test but the
        // UserWidget (us) becomes the receiver for all drag events.
        SkillButton->SetVisibility(ESlateVisibility::HitTestInvisible);

        // Still wire the UButton delegates — they fire via our custom
        // NativeOnMouseButtonDown/Up override, not via the button's own hit.
        SkillButton->OnClicked.AddDynamic(this, &USkillSlotWidget::OnSkillButtonClicked);
        SkillButton->OnPressed.AddDynamic(this, &USkillSlotWidget::OnSkillButtonPressed);
        SkillButton->OnReleased.AddDynamic(this, &USkillSlotWidget::OnSkillButtonReleased);
    }
    else
    {
        UE_LOG(LogGameplay, Error, TEXT("SkillSlotWidget[%d]: SkillButton is NULL - events not bound!"), SlotIndex);
    }

    // All decoration widgets (icons, overlays, borders, text) must be
    // SelfHitTestInvisible — they render but pass mouse/drag events up to us.
    auto MakePassThrough = [](UWidget* W)
    {
        if (W && W->GetVisibility() != ESlateVisibility::Collapsed)
        {
            W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    };
    MakePassThrough(SkillIcon);
    MakePassThrough(CooldownOverlay);
    MakePassThrough(CooldownProgress);
    MakePassThrough(CooldownText);
    MakePassThrough(HotkeyText);
    MakePassThrough(HotkeyBackground);
    MakePassThrough(HighlightBorder);
    MakePassThrough(DropHighlightBorder);

    UpdateVisualState();
}

