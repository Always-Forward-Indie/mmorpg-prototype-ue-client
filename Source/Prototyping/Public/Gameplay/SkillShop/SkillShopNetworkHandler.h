#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
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
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    FSkillShopData        ParseSkillShop(const TSharedPtr<FJsonObject>& Body) const;
    FLearnSkillResultData ParseSkillLearned(const TSharedPtr<FJsonObject>& Root) const;

    UPROPERTY() USkillShopManager*   SkillShopManager   = nullptr;
    UPROPERTY() UNetworkManager*     NetworkManager     = nullptr;
    UPROPERTY() UPlayerSkillManager* PlayerSkillManager = nullptr;

    bool bIsSubscribed = false;
};
