#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InputAction.h"
#include "KeybindManager.generated.h"

class UMyGameInstance;
class UInputMappingContext;
class ABasicPlayer;
struct FKeybindActionEntry;

/** Remappable action descriptor (label + live action reference). */
USTRUCT(BlueprintType)
struct PROTOTYPING_API FKeybindActionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybinds")
	UInputAction* Action = nullptr;

	FKeybindActionEntry() {}
};

/**
 * UKeybindManager
 *
 * Runtime key remapping over the single IMC_Player mapping context.
 * Overrides are kept in-memory on the IMC plus persisted per-action in
 * Game.ini (config=Game), re-applied whenever input is (re)initialized.
 * No packets, no game state — input layer only.
 */
UCLASS(config=Game)
class PROTOTYPING_API UKeybindManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMyGameInstance* InGameInstance);

	/** Build the remappable action registry from the local player. */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	TArray<FKeybindActionEntry> CollectActions(ABasicPlayer* Player) const;

	/** Primary bound key for an action (invalid when unbound). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keybinds")
	FKey GetPrimaryKey(UInputMappingContext* IMC, const UInputAction* Action) const;

	/** Remap the primary key; false + error sound when the key is taken. */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	bool RemapAction(UInputMappingContext* IMC, const UInputAction* Action, const FKey& NewKey);

	/** Drop one override and restore the asset default. */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void ResetAction(UInputMappingContext* IMC, const UInputAction* Action);

	/** Drop all overrides and restore asset defaults. */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void ResetAll(UInputMappingContext* IMC);

	/** Re-apply persisted overrides (call after AddMappingContext). */
	UFUNCTION(BlueprintCallable, Category = "Keybinds")
	void ApplySavedBindings(UInputMappingContext* IMC);

private:
	static FString ActionKey(const UInputAction* Action);

	UPROPERTY()
	UMyGameInstance* GameInstance = nullptr;

	/** action-name -> key-name overrides (persisted in Game.ini). */
	UPROPERTY(config)
	TMap<FString, FString> RemappedKeys;

	/** Session cache of asset defaults (for Reset without a restart). */
	TMap<FString, FKey> DefaultKeys;
};
