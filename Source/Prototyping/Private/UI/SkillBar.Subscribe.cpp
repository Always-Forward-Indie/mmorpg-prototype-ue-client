#include "UI/SkillBarWidget.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Components/HorizontalBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

void USkillBarWidget::SubscribeToSkillManagerEvents()
{
    if (!SkillManager || bEventsSubscribed)
    {
        return;
    }

    SkillManager->OnSkillsInitialized.AddDynamic(this, &USkillBarWidget::OnPlayerSkillsInitialized);
    SkillManager->OnSkillCooldownStarted.AddDynamic(this, &USkillBarWidget::OnSkillCooldownStarted);
    SkillManager->OnSkillReady.AddDynamic(this, &USkillBarWidget::OnSkillReady);
    SkillManager->OnSkillSlotChanged.AddDynamic(this, &USkillBarWidget::OnSkillSlotChanged);

    bEventsSubscribed = true;
}

void USkillBarWidget::UnsubscribeFromSkillManagerEvents()
{
    if (!SkillManager || !bEventsSubscribed)
    {
        return;
    }

    SkillManager->OnSkillsInitialized.RemoveDynamic(this, &USkillBarWidget::OnPlayerSkillsInitialized);
    SkillManager->OnSkillCooldownStarted.RemoveDynamic(this, &USkillBarWidget::OnSkillCooldownStarted);
    SkillManager->OnSkillReady.RemoveDynamic(this, &USkillBarWidget::OnSkillReady);
    SkillManager->OnSkillSlotChanged.RemoveDynamic(this, &USkillBarWidget::OnSkillSlotChanged);

    bEventsSubscribed = false;
}

