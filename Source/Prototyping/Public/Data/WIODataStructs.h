#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WIODataStructs.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EWIOObjectType : uint8
{
	None			UMETA(DisplayName = "None"),
	Examine			UMETA(DisplayName = "Examine"),
	Search			UMETA(DisplayName = "Search"),
	Activate		UMETA(DisplayName = "Activate"),
	UseWithItem		UMETA(DisplayName = "Use With Item"),
	Channeled		UMETA(DisplayName = "Channeled")
};

UENUM(BlueprintType)
enum class EWIOScope : uint8
{
	PerPlayer		UMETA(DisplayName = "Per Player"),
	Global			UMETA(DisplayName = "Global")
};

UENUM(BlueprintType)
enum class EWIOState : uint8
{
	Active			UMETA(DisplayName = "Active"),
	Depleted		UMETA(DisplayName = "Depleted"),
	Disabled		UMETA(DisplayName = "Disabled")
};

// ─────────────────────────────────────────────────────────────────────────────
// World Object spawn data (from spawnWorldObjects packet)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWorldObjectData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ObjectId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	FString Slug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	FString NameKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	EWIOObjectType ObjectType = EWIOObjectType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	EWIOScope Scope = EWIOScope::PerPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	float PosX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	float PosY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	float PosZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	float RotZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ZoneId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	float InteractionRadius = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ChannelTimeSec = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 RespawnSec = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 MinLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 DialogueId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 RequiredItemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	EWIOState CurrentState = EWIOState::Active;
};

// ─────────────────────────────────────────────────────────────────────────────
// Loot item entry (from search interaction result)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWIOLootItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ItemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 Quantity = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Interaction result (from worldObjectInteractResult packet)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWIOInteractResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ObjectId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	FString ErrorCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	FString InteractionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ChannelTimeSec = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	TArray<FWIOLootItem> LootItems;
};

// ─────────────────────────────────────────────────────────────────────────────
// State update (from worldObjectStateUpdate broadcast)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWIOStateUpdate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ObjectId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	EWIOState NewState = EWIOState::Active;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 RespawnSec = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Channel cancelled (from worldObjectChannelCancelled)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWIOChannelCancelled
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	int32 ObjectId = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// DataTable row for WIO Blueprint mapping (slug → actor class)
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWIODefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The actor class to spawn for this WIO slug. Leave None for a generic default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	TSubclassOf<AActor> ActorClass;

	/** Optional static mesh override (used by the default actor if no custom BP). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	TSoftObjectPtr<UStaticMesh> MeshOverride;

	/** Optional material override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	/** Optional interaction icon shown in the prompt widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO")
	TSoftObjectPtr<UTexture2D> InteractionIcon;
};

// ─────────────────────────────────────────────────────────────────────────────
// Localization row for WIO names / descriptions
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWIOLocaleDefinition : public FTableRowBase
{
	GENERATED_BODY()

	/** Localised display name (e.g. "Ancient Altar"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Locale")
	FText DisplayName;

	/** Localised description / flavour text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Locale")
	FText Description;

	/** Localised interaction prompt text (e.g. "Examine", "Search", "Activate"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Locale")
	FText InteractionPrompt;
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: parse WIO enums from string
// ─────────────────────────────────────────────────────────────────────────────

namespace WIOHelpers
{
	inline EWIOObjectType ParseObjectType(const FString& Str)
	{
		if (Str == TEXT("examine"))			return EWIOObjectType::Examine;
		if (Str == TEXT("search"))			return EWIOObjectType::Search;
		if (Str == TEXT("activate"))			return EWIOObjectType::Activate;
		if (Str == TEXT("use_with_item"))	return EWIOObjectType::UseWithItem;
		if (Str == TEXT("channeled"))		return EWIOObjectType::Channeled;
		return EWIOObjectType::None;
	}

	inline EWIOScope ParseScope(const FString& Str)
	{
		if (Str == TEXT("global")) return EWIOScope::Global;
		return EWIOScope::PerPlayer;
	}

	inline EWIOState ParseState(const FString& Str)
	{
		if (Str == TEXT("depleted"))	return EWIOState::Depleted;
		if (Str == TEXT("disabled"))	return EWIOState::Disabled;
		return EWIOState::Active;
	}

	inline FString StateToString(EWIOState State)
	{
		switch (State)
		{
		case EWIOState::Active:		return TEXT("active");
		case EWIOState::Depleted:	return TEXT("depleted");
		case EWIOState::Disabled:	return TEXT("disabled");
		default:					return TEXT("active");
		}
	}
}
