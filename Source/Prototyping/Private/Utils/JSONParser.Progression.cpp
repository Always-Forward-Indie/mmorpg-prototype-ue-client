// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// Experience system parsing functions
FExperienceUpdateStruct JSONParser::DeserializeExperienceUpdate(const TSharedPtr<FJsonObject>& Body)
{
	FExperienceUpdateStruct ExperienceUpdate;
	if (!Body.IsValid()) return ExperienceUpdate;

		Body->TryGetNumberField(TEXT("characterId"), ExperienceUpdate.characterId);

		Body->TryGetNumberField(TEXT("oldLevel"), ExperienceUpdate.oldLevel);

		Body->TryGetNumberField(TEXT("newLevel"), ExperienceUpdate.newLevel);

		Body->TryGetNumberField(TEXT("oldExperience"), ExperienceUpdate.oldExperience);

		Body->TryGetNumberField(TEXT("newExperience"), ExperienceUpdate.newExperience);

		Body->TryGetNumberField(TEXT("experienceChange"), ExperienceUpdate.experienceChange);

		Body->TryGetNumberField(TEXT("expForCurrentLevel"), ExperienceUpdate.expForCurrentLevel);

		Body->TryGetNumberField(TEXT("expForNextLevel"), ExperienceUpdate.expForNextLevel);

		Body->TryGetBoolField(TEXT("levelUp"), ExperienceUpdate.levelUp);

		Body->TryGetStringField(TEXT("reason"), ExperienceUpdate.reason);

		Body->TryGetNumberField(TEXT("sourceId"), ExperienceUpdate.sourceId);

	return ExperienceUpdate;
}

FExperienceUpdateStruct JSONParser::DeserializeExperienceUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeExperienceUpdate failed to parse JSON"));
		return FExperienceUpdateStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeExperienceUpdate missing body"));
		return FExperienceUpdateStruct();
	}

	return JSONParser::DeserializeExperienceUpdate(*BodyPtr);
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const TSharedPtr<FJsonObject>& ProgressionObj)
{
	FPlayerProgressionStruct Progression;
	if (!ProgressionObj.IsValid()) return Progression;

		ProgressionObj->TryGetNumberField(TEXT("characterId"), Progression.characterId);

		ProgressionObj->TryGetNumberField(TEXT("currentLevel"), Progression.currentLevel);

		ProgressionObj->TryGetNumberField(TEXT("currentExperience"), Progression.currentExperience);

		ProgressionObj->TryGetNumberField(TEXT("totalExperience"), Progression.totalExperience);

		ProgressionObj->TryGetNumberField(TEXT("expForNextLevel"), Progression.expForNextLevel);

		ProgressionObj->TryGetNumberField(TEXT("expForCurrentLevel"), Progression.expForCurrentLevel);

		ProgressionObj->TryGetBoolField(TEXT("hasPendingLevelUp"), Progression.bHasPendingLevelUp);

		ProgressionObj->TryGetNumberField(TEXT("pendingLevelGained"), Progression.pendingLevelGained);

	return Progression;
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializePlayerProgression failed to parse JSON"));
		return FPlayerProgressionStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializePlayerProgression missing body"));
		return FPlayerProgressionStruct();
	}

	const TSharedPtr<FJsonObject>* ProgressionPtr = nullptr;
	if (!(*BodyPtr)->TryGetObjectField(TEXT("progression"), ProgressionPtr) || !ProgressionPtr || !(*ProgressionPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializePlayerProgression missing progression"));
		return FPlayerProgressionStruct();
	}

	return JSONParser::DeserializePlayerProgression(*ProgressionPtr);
}

FExperienceGainEventStruct JSONParser::DeserializeExperienceGainEvent(const TSharedPtr<FJsonObject>& EventObj)
{
	FExperienceGainEventStruct Event;
	if (!EventObj.IsValid()) return Event;

		EventObj->TryGetNumberField(TEXT("experienceGained"), Event.experienceGained);

		EventObj->TryGetStringField(TEXT("reason"), Event.reasonText);

		EventObj->TryGetNumberField(TEXT("sourceId"), Event.sourceId);

		EventObj->TryGetStringField(TEXT("sourceName"), Event.sourceName);

	// Parse reason enum from string
	if (!Event.reasonText.IsEmpty())
	{
		if (Event.reasonText.Equals(TEXT("mob_kill"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::MobKill;
		else if (Event.reasonText.Equals(TEXT("quest_complete"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::QuestComplete;
		else if (Event.reasonText.Equals(TEXT("quest_turn_in"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::QuestTurnIn;
		else if (Event.reasonText.Equals(TEXT("discovery"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Discovery;
		else if (Event.reasonText.Equals(TEXT("crafting"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Crafting;
		else if (Event.reasonText.Equals(TEXT("gathering"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Gathering;
		else if (Event.reasonText.Equals(TEXT("pvp_kill"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::PvPKill;
		else if (Event.reasonText.Equals(TEXT("boss_kill"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::BossKill;
		else if (Event.reasonText.Equals(TEXT("group_bonus"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::GroupBonus;
		else if (Event.reasonText.Equals(TEXT("event"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Event;
		else if (Event.reasonText.Equals(TEXT("admin"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Admin;
		else
			Event.reason = EExperienceReason::None;
	}

	// Set timestamp to now (server timestamp parsing could be added here if needed)
	Event.timestamp = FDateTime::Now();

	return Event;
}

FExperienceGainEventStruct JSONParser::DeserializeExperienceGainEvent(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeExperienceGainEvent failed to parse JSON"));
		return FExperienceGainEventStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeExperienceGainEvent missing body"));
		return FExperienceGainEventStruct();
	}

	const TSharedPtr<FJsonObject>* EventPtr = nullptr;
	if (!(*BodyPtr)->TryGetObjectField(TEXT("experienceEvent"), EventPtr) || !EventPtr || !(*EventPtr).IsValid())
	{
		// Try to parse directly from body if no experienceEvent object
		return JSONParser::DeserializeExperienceGainEvent(*BodyPtr);
	}

	return JSONParser::DeserializeExperienceGainEvent(*EventPtr);
}

// Player stats parsing delegates to PlayerAttributeParser (single owner).
FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body)
{
    return PlayerAttributeParser::DeserializePlayerStatsUpdate(Body);
}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const FString& JsonString)
{
	// Single owner: PlayerAttributeParser does the parsing and logging.
	return PlayerAttributeParser::DeserializePlayerStatsUpdate(JsonString);
}

// Player skills system parsers
FPlayerSkillNetworkData JSONParser::DeserializePlayerSkillNetworkData(const TSharedPtr<FJsonObject>& SkillObj)
{
	FPlayerSkillNetworkData SkillData;

	if (SkillObj.IsValid())
	{
			SkillObj->TryGetStringField(TEXT("skillSlug"), SkillData.skillSlug);

			SkillObj->TryGetNumberField(TEXT("skillLevel"), SkillData.skillLevel);

			SkillObj->TryGetNumberField(TEXT("castMs"), SkillData.castMs);

			SkillObj->TryGetNumberField(TEXT("coeff"), SkillData.coeff);

			SkillObj->TryGetNumberField(TEXT("cooldownMs"), SkillData.cooldownMs);

			SkillObj->TryGetNumberField(TEXT("costMp"), SkillData.costMp);

			SkillObj->TryGetNumberField(TEXT("flatAdd"), SkillData.flatAdd);

			SkillObj->TryGetNumberField(TEXT("gcdMs"), SkillData.gcdMs);

			SkillObj->TryGetNumberField(TEXT("maxRange"), SkillData.maxRange);
	}

	return SkillData;
}

TArray<FPlayerSkillNetworkData> JSONParser::DeserializePlayerSkillsArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FPlayerSkillNetworkData> Skills;

	for (const TSharedPtr<FJsonValue>& JsonValue : JsonArray)
	{
		if (JsonValue.IsValid() && JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> SkillObj = JsonValue->AsObject();
			FPlayerSkillNetworkData SkillData = DeserializePlayerSkillNetworkData(SkillObj);
			Skills.Add(SkillData);
		}
	}

	return Skills;
}

FPlayerSkillsInitializationData JSONParser::DeserializePlayerSkillsInitialization(const TSharedPtr<FJsonObject>& Body)
{
	FPlayerSkillsInitializationData InitData;

	if (Body.IsValid())
	{
			Body->TryGetNumberField(TEXT("characterId"), InitData.characterId);

		if (Body->HasField(TEXT("skills")))
		{
			const TArray<TSharedPtr<FJsonValue>>* SkillsArray;
			if (Body->TryGetArrayField(TEXT("skills"), SkillsArray))
			{
				InitData.skills = DeserializePlayerSkillsArray(*SkillsArray);
			}
		}
	}

	return InitData;
}

FPlayerSkillsInitializationData JSONParser::DeserializePlayerSkillsInitialization(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	FPlayerSkillsInitializationData InitData;

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializePlayerSkillsInitialization failed to parse JSON"));
		return InitData;
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializePlayerSkillsInitialization missing body"));
		return InitData;
	}

	return DeserializePlayerSkillsInitialization(*BodyPtr);
}

