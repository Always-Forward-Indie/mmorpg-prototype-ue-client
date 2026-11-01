#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/PanelWidget.h"
#include "Data/DataStructs.h"
#include "Utils/NetCore/CombatLogRule.h"
#include "CombatLogWidget.generated.h"

class UCombatLogManager;
class UScrollBox;
class UCheckBox;
class UVerticalBox;
class UPanelWidget;
struct FCombatLogEntry;

/**
 * UCombatLogWidget
 *
 * History window for damage / heal / loot / XP / system lines — the same
 * role as the chat log, but fed by UCombatLogManager (authoritative server
 * facts only).
 *
 * Blueprint layout (optional): create a BP subclass, bind these names,
 * assign it to UIManager.CombatLogWidgetClass. Bind both or none — a
 * scroll box without a filter row simply shows unfiltered lines:
 *   Log_Scroll_Box  UScrollBox    line list (AppendEntry targets it)
 *   Log_Filter_Row  any UPanelWidget (HorizontalBox recommended) —
 *                   the five channel checkboxes are built into it in code
 * Placement comes from the BP root's own anchors; the LogSize/LogPosition/
 * LogAnchors/LogAlignment tunables apply to the code fallback only.
 * With no subclass assigned the whole layout is built in C++ (headless-safe).
 *
 * Clicks: the scroll area is Visible (consistent with the chat window);
 * only the filter checkboxes are interactive.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UCombatLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind a log manager: replays history, then streams new lines. */
	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void BindToManager(UCombatLogManager* InManager);

	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void ClearLog();

	/** Show/hide one channel (drives the filter checkboxes). */
	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void SetCategoryVisible(ECombatLogCategory Category, bool bVisible);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat Log")
	bool IsCategoryVisible(ECombatLogCategory Category) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Append one entry (filter-checked). Called for history + live lines. */
	void AppendEntry(const FCombatLogEntry& Entry);
	/** Full rebuild from the manager history (filter toggles). */
	void RebuildVisible();

	UFUNCTION() void HandleLogEntryAdded(const FCombatLogEntry& Entry);
	UFUNCTION() void HandleFilterChanged(bool bIsChecked);
	UFUNCTION() void HandleUserScrolled(float CurrentOffset);

	// --- Blueprint layout slots (BindWidgetOptional — see class doc) ---
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* Log_Scroll_Box = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* Log_Filter_Row = nullptr;

	// --- C++-built layout (no BP bindings required) ---
	UPROPERTY() UScrollBox* LogScroll = nullptr;
	UPROPERTY() TArray<UTextBlock*> LineWidgets;
	UPROPERTY() TMap<ECombatLogCategory, UCheckBox*> FilterBoxes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FVector2D LogSize = FVector2D(400.f, 280.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FVector2D LogPosition = FVector2D(8.f, -140.f);

	// UI-14: anchor/alignment override (default: left edge, vertically centered).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FAnchors LogAnchors = FAnchors(0.f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FVector2D LogAlignment = FVector2D(0.f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	int32 MaxLines = FCombatLogRule::MaxWidgetLines;

	/** Prefix lines with [HH:MM:SS]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	bool bShowTimestamps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	int32 LineFontSize = 12;

	// T-02: BP-tunable channel colours (mirror FCombatLogRule::ColorFor).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	bool bOverrideCategoryColors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log", meta = (EditCondition = "bOverrideCategoryColors"))
	FLinearColor DamageColor = FLinearColor(1.0f, 0.35f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log", meta = (EditCondition = "bOverrideCategoryColors"))
	FLinearColor HealColor = FLinearColor(0.35f, 1.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log", meta = (EditCondition = "bOverrideCategoryColors"))
	FLinearColor LootColor = FLinearColor(1.0f, 0.85f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log", meta = (EditCondition = "bOverrideCategoryColors"))
	FLinearColor ExperienceColor = FLinearColor(0.45f, 0.80f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log", meta = (EditCondition = "bOverrideCategoryColors"))
	FLinearColor SystemColor = FLinearColor(0.75f, 0.75f, 0.75f);

private:
	UPROPERTY() UCombatLogManager* LogManager = nullptr;

	uint8 FilterMask = FCombatLogRule::AllMask();
	bool bFollowTail = true;

	void BuildLayout();
	UCheckBox* AddFilterCheck(ECombatLogCategory Category, const FString& Label, UPanelWidget* Row);
	void TrimLines();
	bool IsAtBottom() const;
	FLinearColor ColorFor(ECombatLogCategory Category) const;

	// Wiring happens once (NativeConstruct may re-run on re-add to viewport).
	bool bWired = false;
};
