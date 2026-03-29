#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "EquipmentNetworkHandler.generated.h"

class UNetworkManager;
class UEquipmentManager;

/**
 * EquipmentNetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes equipment-related packets
 * (EQUIPMENT_STATE, EQUIP_RESULT, WEIGHT_STATUS, charAttributesUpdate) to EquipmentManager.
 */
UCLASS()
class PROTOTYPING_API UEquipmentNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UEquipmentManager* InEquipmentManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Equipment Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Equipment Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    FEquipmentStateData  ParseEquipmentState(const TSharedPtr<FJsonObject>& Body) const;
    FEquipResultData     ParseEquipResult(const TSharedPtr<FJsonObject>& Root,
                                          const TSharedPtr<FJsonObject>& Body,
                                          const FString& Message) const;
    FWeightStatusData    ParseWeightStatus(const TSharedPtr<FJsonObject>& Body) const;
    TArray<FAttributeDataStruct> ParseAttributes(const TSharedPtr<FJsonObject>& Body) const;

    UPROPERTY()
    UEquipmentManager* EquipmentManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
