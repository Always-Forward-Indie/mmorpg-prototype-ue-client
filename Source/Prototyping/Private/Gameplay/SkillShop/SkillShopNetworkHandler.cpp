#include "Gameplay/SkillShop/SkillShopNetworkHandler.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void USkillShopNetworkHandler::Initialize(USkillShopManager* InSkillShopManager,
                                           UNetworkManager*   InNetworkManager,
                                           UPlayerSkillManager* InPlayerSkillManager)
{
    if (!InSkillShopManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillShopNetworkHandler: Initialize called with null parameters"));
        return;
    }
    SkillShopManager   = InSkillShopManager;
    NetworkManager     = InNetworkManager;
    PlayerSkillManager = InPlayerSkillManager; // may be null early on; handled safely
}

void USkillShopNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &USkillShopNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void USkillShopNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &USkillShopNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void USkillShopNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !SkillShopManager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    // Server sends 'skillShop' for direct openSkillShop requests
    // and 'openSkillShop' when triggered from a dialogue action — treat both the same.
    if (EventType != TEXT("skillShop") &&
        EventType != TEXT("openSkillShop") &&
        EventType != TEXT("skill_learned") &&
        EventType != TEXT("learn_skill_failed"))
    {
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    if (EventType == TEXT("skillShop") || EventType == TEXT("openSkillShop"))
    {
        UE_LOG(LogTemp, Log, TEXT("[SkillShop] <<< %s received. Raw: %s"), *EventType, *ReceivedData);

        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SkillShop] %s packet missing 'body' field"), *EventType);
            return;
        }

        FSkillShopData Shop = ParseSkillShop(*BodyPtr);
        UE_LOG(LogTemp, Log, TEXT("[SkillShop] Parsed skillShop: npcId=%d npcSlug='%s' freeSkillPoints=%d goldBalance=%d skills=%d"),
            Shop.npcId, *Shop.npcSlug, Shop.freeSkillPoints, Shop.goldBalance, Shop.skills.Num());

        for (int32 i = 0; i < Shop.skills.Num(); ++i)
        {
            const FSkillShopSkillData& S = Shop.skills[i];
            UE_LOG(LogTemp, Log,
                TEXT("[SkillShop]   [%d] slug='%s' name='%s' lvl=%d sp=%d gold=%d book=%d "
                     "isLearned=%d canLearn=%d prereqMet=%d levelMet=%d spMet=%d goldMet=%d bookMet=%d"),
                i, *S.skillSlug, *S.skillName, S.requiredLevel, S.spCost, S.goldCost, S.requiresBook ? 1 : 0,
                S.isLearned ? 1 : 0, S.canLearn ? 1 : 0,
                S.prereqMet ? 1 : 0, S.levelMet ? 1 : 0, S.spMet ? 1 : 0, S.goldMet ? 1 : 0, S.bookMet ? 1 : 0);
        }

        SkillShopManager->OnSkillShopReceived(Shop);
    }
    else if (EventType == TEXT("skill_learned"))
    {
        UE_LOG(LogTemp, Log, TEXT("[SkillShop] <<< skill_learned received. Raw: %s"), *ReceivedData);

        FLearnSkillResultData Result = ParseSkillLearned(Root);
        Result.bSuccess = true;

        UE_LOG(LogTemp, Log, TEXT("[SkillShop] Parsed skill_learned: slug='%s' name='%s' newFreeSkillPoints=%d"),
            *Result.skillSlug, *Result.skillName, Result.newFreeSkillPoints);

        // Add to PlayerSkillManager so the hotbar updates immediately
        if (PlayerSkillManager && IsValid(PlayerSkillManager))
        {
            PlayerSkillManager->AddLearnedSkill(Result.skillData);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SkillShop] PlayerSkillManager is null — hotbar will NOT be updated"));
        }

        SkillShopManager->OnSkillLearnedReceived(Result);
    }
    else if (EventType == TEXT("learn_skill_failed"))
    {
        UE_LOG(LogTemp, Log, TEXT("[SkillShop] <<< learn_skill_failed received. Raw: %s"), *ReceivedData);

        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SkillShop] learn_skill_failed packet missing 'body' field"));
            return;
        }

        FString SkillSlug, Reason;
        (*BodyPtr)->TryGetStringField(TEXT("skillSlug"), SkillSlug);
        (*BodyPtr)->TryGetStringField(TEXT("reason"),    Reason);

        // Also try top-level body fields (protocol sends "type":"learn_skill_failed")
        if (Reason.IsEmpty())
            (*BodyPtr)->TryGetStringField(TEXT("type"), Reason);

        UE_LOG(LogTemp, Warning, TEXT("[SkillShop] Learn failed: slug='%s' reason='%s'"), *SkillSlug, *Reason);

        SkillShopManager->OnSkillLearnFailedReceived(SkillSlug, Reason);
    }
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FSkillShopData USkillShopNetworkHandler::ParseSkillShop(const TSharedPtr<FJsonObject>& Body) const
{
    FSkillShopData Shop;
    Body->TryGetNumberField(TEXT("npcId"),           Shop.npcId);
    Body->TryGetStringField(TEXT("npcSlug"),         Shop.npcSlug);
    Body->TryGetNumberField(TEXT("freeSkillPoints"), Shop.freeSkillPoints);
    Body->TryGetNumberField(TEXT("goldBalance"),     Shop.goldBalance);

    const TArray<TSharedPtr<FJsonValue>>* SkillsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("skills"), SkillsArray)) return Shop;

    for (const TSharedPtr<FJsonValue>& Val : *SkillsArray)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FSkillShopSkillData Skill;
        Obj->TryGetNumberField(TEXT("skillId"),                Skill.skillId);
        Obj->TryGetStringField(TEXT("skillSlug"),              Skill.skillSlug);
        Obj->TryGetStringField(TEXT("skillName"),              Skill.skillName);
        Obj->TryGetStringField(TEXT("description"),            Skill.description);
        Obj->TryGetBoolField  (TEXT("isPassive"),              Skill.isPassive);
        Obj->TryGetNumberField(TEXT("requiredLevel"),          Skill.requiredLevel);
        Obj->TryGetNumberField(TEXT("spCost"),                 Skill.spCost);
        Obj->TryGetNumberField(TEXT("goldCost"),               Skill.goldCost);
        Obj->TryGetBoolField  (TEXT("requiresBook"),           Skill.requiresBook);
        Obj->TryGetNumberField(TEXT("bookItemId"),             Skill.bookItemId);
        Obj->TryGetStringField(TEXT("prerequisiteSkillSlug"),  Skill.prerequisiteSkillSlug);
        Obj->TryGetBoolField  (TEXT("isLearned"),              Skill.isLearned);
        Obj->TryGetBoolField  (TEXT("canLearn"),               Skill.canLearn);
        Obj->TryGetBoolField  (TEXT("prereqMet"),              Skill.prereqMet);
        Obj->TryGetBoolField  (TEXT("levelMet"),               Skill.levelMet);
        Obj->TryGetBoolField  (TEXT("spMet"),                  Skill.spMet);
        Obj->TryGetBoolField  (TEXT("goldMet"),                Skill.goldMet);
        Obj->TryGetBoolField  (TEXT("bookMet"),                Skill.bookMet);

        Shop.skills.Add(Skill);
    }

    return Shop;
}

FLearnSkillResultData USkillShopNetworkHandler::ParseSkillLearned(const TSharedPtr<FJsonObject>& Root) const
{
    FLearnSkillResultData Result;

    // The skill_learned event can come as a top-level body or as an action notification
    // Protocol: top-level keys at body level
    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    const TSharedPtr<FJsonObject>* SearchObj = nullptr;

    // Try body first
    if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        SearchObj = BodyPtr;
    }
    else
    {
        // Some action notifications embed directly in root
        const TSharedPtr<FJsonObject>* RootPtr = &Root;
        SearchObj = RootPtr;
    }

    if (!SearchObj) return Result;

    (*SearchObj)->TryGetStringField(TEXT("skillSlug"),         Result.skillSlug);
    (*SearchObj)->TryGetStringField(TEXT("skillName"),         Result.skillName);
    (*SearchObj)->TryGetBoolField  (TEXT("isPassive"),         Result.isPassive);
    (*SearchObj)->TryGetNumberField(TEXT("newFreeSkillPoints"),Result.newFreeSkillPoints);

    // Parse nested skillData object
    const TSharedPtr<FJsonObject>* SkillDataObj = nullptr;
    if ((*SearchObj)->TryGetObjectField(TEXT("skillData"), SkillDataObj))
    {
        (*SkillDataObj)->TryGetStringField(TEXT("skillSlug"),  Result.skillData.skillSlug);
        (*SkillDataObj)->TryGetNumberField(TEXT("skillId"),    Result.skillData.skillLevel); // skillId not in FPlayerSkillNetworkData, skip
        (*SkillDataObj)->TryGetNumberField(TEXT("currentLevel"), Result.skillData.skillLevel);

        // skillData doesn't have cast/cooldown in skill_learned response; keep defaults (0)
        // They'll be properly populated when initializePlayerSkills is sent on next session
    }
    else
    {
        // No skillData sub-object — fill slug from top level
        Result.skillData.skillSlug = Result.skillSlug;
        Result.skillData.skillLevel = 1;
    }

    return Result;
}
