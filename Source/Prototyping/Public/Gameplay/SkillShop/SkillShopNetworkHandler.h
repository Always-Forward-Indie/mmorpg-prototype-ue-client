#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "SkillShopNetworkHandler.generated.h"

class UNetworkManager;
class USkillShopManager;
class UPlayerSkillManager;

/**
 * SkillShopNetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes:
 *   "skillShop"          → SkillShopManager::OnSkillShopReceived
 *   "skill_learned"      → SkillShopManager::OnSkillLearnedReceived
 *                        → PlayerSkillManager::AddLearnedSkill  (hotbar update)
 *   "learn_skill_failed" → SkillShopManager::OnSkillLearnFailedReceived
 */
UCLASS()
class PROTOTYPING_API USkillShopNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(USkillShopManager* InSkillShopManager,
                    UNetworkManager*   InNetworkManager,
                    UPlayerSkillManager* InPlayerSkillManager);

    UFUNCTION(BlueprintCallable, Category = "Skill Shop Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Skill Shop Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    FSkillShopData        ParseSkillShop(const TSharedPtr<FJsonObject>& Body) const;
    FLearnSkillResultData ParseSkillLearned(const TSharedPtr<FJsonObject>& Root) const;

    UPROPERTY() USkillShopManager*   SkillShopManager   = nullptr;
    UPROPERTY() UNetworkManager*     NetworkManager     = nullptr;
    UPROPERTY() UPlayerSkillManager* PlayerSkillManager = nullptr;

    bool bIsSubscribed = false;
};
