#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMenuBarWidget.generated.h"

// Forward declarations
class UButton;
class UHintTooltipWidget;

// Delegates — each button notifies UIManager (or whoever is listening)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarInventoryClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarEquipmentClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarQuestJournalClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarSkillsClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarStatsClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarBestiaryClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarTitlesClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarReputationClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarEmoteClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuBarMenuClicked);

/**
 * Bottom action-bar with shortcut icons for the main game windows:
 *   Inventory | Equipment | Quest Journal | Skills
 *
 * Purely a presenter — business logic lives in UIManager.
 * Bind the On*Clicked delegates in UIManager after creation.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UGameMenuBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarInventoryClicked OnInventoryClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarEquipmentClicked OnEquipmentClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarQuestJournalClicked OnQuestJournalClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarSkillsClicked OnSkillsClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarStatsClicked OnStatsClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarBestiaryClicked OnBestiaryClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarTitlesClicked OnTitlesClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarReputationClicked OnReputationClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarEmoteClicked OnEmoteClicked;
    UPROPERTY(BlueprintAssignable, Category = "Game Menu Bar|Events")
    FOnMenuBarMenuClicked OnMenuClicked;

protected:
    virtual void NativeConstruct() override;

    /** Blueprint widget class for the hint tooltip. Assign a UHintTooltipWidget subclass. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    TSubclassOf<UHintTooltipWidget> HintTooltipClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText InventoryHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText EquipmentHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText QuestJournalHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText SkillsHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText StatsHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText BestiaryHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText TitlesHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText ReputationHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText EmoteHint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Menu Bar|Hints")
    FText MenuHint;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Inventory;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Equipment;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_QuestJournal;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Skills;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Stats;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Bestiary;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Titles;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Reputation;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Emote;
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Game Menu Bar")
    UButton* Btn_Menu;

private:
    /** Creates and assigns a hint tooltip to a button if its hint text is non-empty. */
    void SetupButtonHint(UButton* Btn, const FText& HintText);

    UFUNCTION() void HandleInventoryClicked();
    UFUNCTION() void HandleEquipmentClicked();
    UFUNCTION() void HandleQuestJournalClicked();
    UFUNCTION() void HandleSkillsClicked();
    UFUNCTION() void HandleStatsClicked();
    UFUNCTION() void HandleBestiaryClicked();
    UFUNCTION() void HandleTitlesClicked();
    UFUNCTION() void HandleReputationClicked();
    UFUNCTION() void HandleEmoteClicked();
    UFUNCTION() void HandleMenuClicked();
};
