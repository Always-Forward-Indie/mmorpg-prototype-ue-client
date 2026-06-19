#include "UI/GameMenuBarWidget.h"
#include "UI/HintTooltipWidget.h"
#include "Components/Button.h"

void UGameMenuBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Inventory)    Btn_Inventory->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleInventoryClicked);
    if (Btn_Equipment)    Btn_Equipment->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleEquipmentClicked);
    if (Btn_QuestJournal) Btn_QuestJournal->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleQuestJournalClicked);
    if (Btn_Skills)       Btn_Skills->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleSkillsClicked);
    if (Btn_Stats)        Btn_Stats->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleStatsClicked);
    if (Btn_Bestiary)     Btn_Bestiary->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleBestiaryClicked);
    if (Btn_Titles)       Btn_Titles->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleTitlesClicked);
    if (Btn_Reputation)   Btn_Reputation->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleReputationClicked);
    if (Btn_Emote)        Btn_Emote->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleEmoteClicked);
    if (Btn_Menu)         Btn_Menu->OnClicked.AddDynamic(this, &UGameMenuBarWidget::HandleMenuClicked);

    SetupButtonHint(Btn_Inventory,    InventoryHint);
    SetupButtonHint(Btn_Equipment,    EquipmentHint);
    SetupButtonHint(Btn_QuestJournal, QuestJournalHint);
    SetupButtonHint(Btn_Skills,       SkillsHint);
    SetupButtonHint(Btn_Stats,        StatsHint);
    SetupButtonHint(Btn_Bestiary,     BestiaryHint);
    SetupButtonHint(Btn_Titles,       TitlesHint);
    SetupButtonHint(Btn_Reputation,   ReputationHint);
    SetupButtonHint(Btn_Emote,        EmoteHint);
    SetupButtonHint(Btn_Menu,         MenuHint);
}

void UGameMenuBarWidget::SetupButtonHint(UButton* Btn, const FText& HintText)
{
    if (!Btn || HintText.IsEmpty() || !HintTooltipClass) return;

    UHintTooltipWidget* Tip = CreateWidget<UHintTooltipWidget>(this, HintTooltipClass);
    if (Tip)
    {
        Tip->SetHintText(HintText);
        Btn->SetToolTip(Tip);
    }
}

void UGameMenuBarWidget::HandleInventoryClicked()    { OnInventoryClicked.Broadcast(); }
void UGameMenuBarWidget::HandleEquipmentClicked()    { OnEquipmentClicked.Broadcast(); }
void UGameMenuBarWidget::HandleQuestJournalClicked() { OnQuestJournalClicked.Broadcast(); }
void UGameMenuBarWidget::HandleSkillsClicked()       { OnSkillsClicked.Broadcast(); }
void UGameMenuBarWidget::HandleStatsClicked()        { OnStatsClicked.Broadcast(); }
void UGameMenuBarWidget::HandleBestiaryClicked()     { OnBestiaryClicked.Broadcast(); }
void UGameMenuBarWidget::HandleTitlesClicked()       { OnTitlesClicked.Broadcast(); }
void UGameMenuBarWidget::HandleReputationClicked()   { OnReputationClicked.Broadcast(); }
void UGameMenuBarWidget::HandleEmoteClicked()        { OnEmoteClicked.Broadcast(); }
void UGameMenuBarWidget::HandleMenuClicked()         { OnMenuClicked.Broadcast(); }
