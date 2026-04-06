#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "SkillShopManager.generated.h"

class UNetworkManager;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillShopOpened,      const FSkillShopData&,       ShopData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillLearned,          const FLearnSkillResultData&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillLearnFailed,     const FString&,              SkillSlug,
                                                                       const FString&,              Reason);

/**
 * SkillShopManager
 *
 * Owns the current skill-trainer shop state.
 * Sends openSkillShop / requestLearnSkill requests to the chunk server
 * and fires delegates so USkillShopWidget can react.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API USkillShopManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // --- Outgoing requests ---

    UFUNCTION(BlueprintCallable, Category = "Skill Shop")
    void RequestOpenSkillShop(int32 NpcId, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Skill Shop")
    void RequestLearnSkill(int32 NpcId, const FString& SkillSlug);

    // --- Called by SkillShopNetworkHandler ---

    void OnSkillShopReceived(const FSkillShopData& ShopData);
    void OnSkillLearnedReceived(const FLearnSkillResultData& Result);
    void OnSkillLearnFailedReceived(const FString& SkillSlug, const FString& Reason);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Shop")
    FSkillShopData GetCurrentShop() const { return CurrentShop; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Skill Shop")
    bool IsShopOpen() const { return bIsOpen; }

    // --- Delegates ---

    UPROPERTY(BlueprintAssignable, Category = "Skill Shop Events")
    FOnSkillShopOpened OnSkillShopOpenedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Skill Shop Events")
    FOnSkillLearned OnSkillLearnedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Skill Shop Events")
    FOnSkillLearnFailed OnSkillLearnFailedDelegate;

private:
    FSkillShopData CurrentShop;
    bool bIsOpen = false;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;
};
