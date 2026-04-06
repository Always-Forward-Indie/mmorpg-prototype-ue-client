#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void USkillShopManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillShopManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
}

// ---------------------------------------------------------------------------
// Outgoing requests
// ---------------------------------------------------------------------------

void USkillShopManager::RequestOpenSkillShop(int32 NpcId, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("openSkillShop"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());

    Body->SetNumberField(TEXT("npcId"), NpcId);
    Body->SetNumberField(TEXT("posX"),  PlayerPosition.X);
    Body->SetNumberField(TEXT("posY"),  PlayerPosition.Y);
    Body->SetNumberField(TEXT("posZ"),  PlayerPosition.Z);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    NetworkManager->SendDataToChunkServer(Payload);
}

void USkillShopManager::RequestLearnSkill(int32 NpcId, const FString& SkillSlug)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("requestLearnSkill"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());

    Body->SetNumberField(TEXT("npcId"),      NpcId);
    Body->SetStringField(TEXT("skillSlug"),  SkillSlug);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Log, TEXT("[SkillShop] >>> requestLearnSkill: npcId=%d skillSlug='%s' "),
        NpcId, *SkillSlug);
    UE_LOG(LogTemp, Log, TEXT("[SkillShop] >>> requestLearnSkill payload: %s"), *Payload);

    NetworkManager->SendDataToChunkServer(Payload);
}

// ---------------------------------------------------------------------------
// Called by SkillShopNetworkHandler
// ---------------------------------------------------------------------------

void USkillShopManager::OnSkillShopReceived(const FSkillShopData& ShopData)
{
    CurrentShop = ShopData;
    bIsOpen     = true;
    OnSkillShopOpenedDelegate.Broadcast(CurrentShop);
}

void USkillShopManager::OnSkillLearnedReceived(const FLearnSkillResultData& Result)
{
    // Mark skill as learned in cached shop (so widget can update icons/buttons)
    for (FSkillShopSkillData& Skill : CurrentShop.skills)
    {
        if (Skill.skillSlug.Equals(Result.skillSlug, ESearchCase::IgnoreCase))
        {
            Skill.isLearned = true;
            Skill.canLearn  = false;
            break;
        }
    }
    // Update SP balance
    CurrentShop.freeSkillPoints = Result.newFreeSkillPoints;

    OnSkillLearnedDelegate.Broadcast(Result);
}

void USkillShopManager::OnSkillLearnFailedReceived(const FString& SkillSlug, const FString& Reason)
{
    OnSkillLearnFailedDelegate.Broadcast(SkillSlug, Reason);
}
