#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "PlayerInspectManager.generated.h"

class UNetworkManager;
class UMyGameInstance;

/** One equipped slot in a public inspect snapshot. */
USTRUCT(BlueprintType)
struct PROTOTYPING_API FPlayerInspectGearEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FString SlotSlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FString ItemSlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	int32 ItemId = 0;

	FPlayerInspectGearEntry() {}
};

/** Read-only public snapshot of another player (no inventory/gold/position). */
USTRUCT(BlueprintType)
struct PROTOTYPING_API FPlayerInspectData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	int32 CharacterId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	int32 CharacterLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FString ClassSlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FString RaceSlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FString GenderSlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	TArray<FPlayerInspectGearEntry> Gear;

	FPlayerInspectData() {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerInspectReceived, const FPlayerInspectData&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerInspectError, const FString&, Reason);

/**
 * UPlayerInspectManager
 *
 * Read-only snapshot flow: RequestInspect → chunk `playerInspectRequest` →
 * `playerInspect` → OnInspectReceived. Single in-flight request (newest
 * wins); responses for stale targets are still delivered (window shows the
 * latest arrival — the menu opens one target at a time).
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UPlayerInspectManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void RequestInspect(int32 TargetCharacterId);

	// --- Called by UPlayerInspectNetworkHandler ---
	void OnInspectReceived(const FPlayerInspectData& Data);
	void OnInspectError(const FString& Reason);

	UPROPERTY(BlueprintAssignable, Category = "Inspect")
	FOnPlayerInspectReceived OnInspectReceivedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Inspect")
	FOnPlayerInspectError OnInspectErrorDelegate;

private:
	UPROPERTY() UNetworkManager* NetworkManager = nullptr;
	UPROPERTY() UMyGameInstance* GameInstance = nullptr;

	void SendPacket(const FString& JsonPayload);
};
