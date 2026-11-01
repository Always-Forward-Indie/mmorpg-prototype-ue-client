#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Data/DataStructs.h"
#include "Utils/NetCore/CombatLogRule.h"
#include "CombatLogWidget.generated.h"

class UCombatLogManager;
class UScrollBox;
class UCheckBox;
class UVerticalBox;
struct FCombatLogEntry;

/**
 * UCombatLogWidget
 *
 * History window for damage / heal / loot / XP / system lines — the same
 * role as the chat log, but fed by UCombatLogManager (authoritative server
 * facts only). Builds its whole layout in C++ (filter row + scrollback),
 * so it works with zero Blueprint setup; a BP subclass may still restyle it.
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

	// --- C++-built layout (no BP bindings required) ---
	UPROPERTY() UScrollBox* LogScroll = nullptr;
	UPROPERTY() TArray<UTextBlock*> LineWidgets;
	UPROPERTY() TMap<ECombatLogCategory, UCheckBox*> FilterBoxes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FVector2D LogSize = FVector2D(400.f, 280.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FVector2D LogPosition = FVector2D(8.f, -140.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	int32 MaxLines = FCombatLogRule::MaxWidgetLines;

private:
	UPROPERTY() UCombatLogManager* LogManager = nullptr;

	uint8 FilterMask = FCombatLogRule::AllMask();
	bool bFollowTail = true;

	void BuildLayout();
	UCheckBox* AddFilterCheck(ECombatLogCategory Category, const FString& Label, class UHorizontalBox* Row);
	void TrimLines();
	bool IsAtBottom() const;
};
