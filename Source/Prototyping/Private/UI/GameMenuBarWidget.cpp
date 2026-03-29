#include "UI/GameMenuBarWidget.h"
#include "Components/Button.h"

void UGameMenuBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Inventory)
        Btn_Inventory->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleInventoryClicked);

    if (Btn_Equipment)
        Btn_Equipment->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleEquipmentClicked);

    if (Btn_QuestJournal)
        Btn_QuestJournal->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleQuestJournalClicked);

    if (Btn_Skills)
        Btn_Skills->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleSkillsClicked);

    if (Btn_Stats)
        Btn_Stats->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleStatsClicked);

    if (Btn_Bestiary)
        Btn_Bestiary->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleBestiaryClicked);

    if (Btn_Menu)
        Btn_Menu->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleMenuClicked);
}

void UGameMenuBarWidget::HandleInventoryClicked()
{
    OnInventoryClicked.Broadcast();
}

void UGameMenuBarWidget::HandleEquipmentClicked()
{
    OnEquipmentClicked.Broadcast();
}

void UGameMenuBarWidget::HandleQuestJournalClicked()
{
    OnQuestJournalClicked.Broadcast();
}

void UGameMenuBarWidget::HandleSkillsClicked()
{
    OnSkillsClicked.Broadcast();
}

void UGameMenuBarWidget::HandleStatsClicked()
{
    OnStatsClicked.Broadcast();
}

void UGameMenuBarWidget::HandleBestiaryClicked()
{
    OnBestiaryClicked.Broadcast();
}

void UGameMenuBarWidget::HandleMenuClicked()
{
    OnMenuClicked.Broadcast();
}
