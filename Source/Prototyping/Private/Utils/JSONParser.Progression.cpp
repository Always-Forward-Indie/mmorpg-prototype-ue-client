// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
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

	if (Body->HasField(TEXT("characterId")))
		ExperienceUpdate.characterId = Body->GetIntegerField(TEXT("characterId"));

	if (Body->HasField(TEXT("oldLevel")))
		ExperienceUpdate.oldLevel = Body->GetIntegerField(TEXT("oldLevel"));

	if (Body->HasField(TEXT("newLevel")))
		ExperienceUpdate.newLevel = Body->GetIntegerField(TEXT("newLevel"));

	if (Body->HasField(TEXT("oldExperience")))
		ExperienceUpdate.oldExperience = Body->GetIntegerField(TEXT("oldExperience"));

	if (Body->HasField(TEXT("newExperience")))
		ExperienceUpdate.newExperience = Body->GetIntegerField(TEXT("newExperience"));

	if (Body->HasField(TEXT("experienceChange")))
		ExperienceUpdate.experienceChange = Body->GetIntegerField(TEXT("experienceChange"));

	if (Body->HasField(TEXT("expForCurrentLevel")))
		ExperienceUpdate.expForCurrentLevel = Body->GetIntegerField(TEXT("expForCurrentLevel"));

	if (Body->HasField(TEXT("expForNextLevel")))
		ExperienceUpdate.expForNextLevel = Body->GetIntegerField(TEXT("expForNextLevel"));

	if (Body->HasField(TEXT("levelUp")))
		ExperienceUpdate.levelUp = Body->GetBoolField(TEXT("levelUp"));

	if (Body->HasField(TEXT("reason")))
		ExperienceUpdate.reason = Body->GetStringField(TEXT("reason"));

	if (Body->HasField(TEXT("sourceId")))
		ExperienceUpdate.sourceId = Body->GetIntegerField(TEXT("sourceId"));

	return ExperienceUpdate;
}

FExperienceUpdateStruct JSONParser::DeserializeExperienceUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FExperienceUpdateStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FExperienceUpdateStruct();

	return JSONParser::DeserializeExperienceUpdate(Body);
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const TSharedPtr<FJsonObject>& ProgressionObj)
{
	FPlayerProgressionStruct Progression;
	if (!ProgressionObj.IsValid()) return Progression;

	if (ProgressionObj->HasField(TEXT("characterId")))
		Progression.characterId = ProgressionObj->GetIntegerField(TEXT("characterId"));

	if (ProgressionObj->HasField(TEXT("currentLevel")))
		Progression.currentLevel = ProgressionObj->GetIntegerField(TEXT("currentLevel"));

	if (ProgressionObj->HasField(TEXT("currentExperience")))
		Progression.currentExperience = ProgressionObj->GetIntegerField(TEXT("currentExperience"));

	if (ProgressionObj->HasField(TEXT("totalExperience")))
		Progression.totalExperience = ProgressionObj->GetIntegerField(TEXT("totalExperience"));

	if (ProgressionObj->HasField(TEXT("expForNextLevel")))
		Progression.expForNextLevel = ProgressionObj->GetIntegerField(TEXT("expForNextLevel"));

	if (ProgressionObj->HasField(TEXT("expForCurrentLevel")))
		Progression.expForCurrentLevel = ProgressionObj->GetIntegerField(TEXT("expForCurrentLevel"));

	if (ProgressionObj->HasField(TEXT("hasPendingLevelUp")))
		Progression.bHasPendingLevelUp = ProgressionObj->GetBoolField(TEXT("hasPendingLevelUp"));

	if (ProgressionObj->HasField(TEXT("pendingLevelGained")))
		Progression.pendingLevelGained = ProgressionObj->GetIntegerField(TEXT("pendingLevelGained"));

	return Progression;
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FPlayerProgressionStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FPlayerProgressionStruct();

	TSharedPtr<FJsonObject> Progression = Body->GetObjectField(TEXT("progression"));
	if (!Progression.IsValid())
		return FPlayerProgressionStruct();

	return JSONParser::DeserializePlayerProgression(Progression);
}

FExperienceGainEventStruct JSONParser::DeserializeExperienceGainEvent(const TSharedPtr<FJsonObject>& EventObj)
{
	FExperienceGainEventStruct Event;
	if (!EventObj.IsValid()) return Event;

	if (EventObj->HasField(TEXT("experienceGained")))
		Event.experienceGained = EventObj->GetIntegerField(TEXT("experienceGained"));

	if (EventObj->HasField(TEXT("reason")))
		Event.reasonText = EventObj->GetStringField(TEXT("reason"));

	if (EventObj->HasField(TEXT("sourceId")))
		Event.sourceId = EventObj->GetIntegerField(TEXT("sourceId"));

	if (EventObj->HasField(TEXT("sourceName")))
		Event.sourceName = EventObj->GetStringField(TEXT("sourceName"));

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
		return FExperienceGainEventStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FExperienceGainEventStruct();

	TSharedPtr<FJsonObject> Event = Body->GetObjectField(TEXT("experienceEvent"));
	if (!Event.IsValid())
	{
		// Try to parse directly from body if no experienceEvent object
		return JSONParser::DeserializeExperienceGainEvent(Body);
	}

	return JSONParser::DeserializeExperienceGainEvent(Event);
}

//FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const FString& JsonString)
//{
//    return PlayerAttributeParser::DeserializePlayerStatsUpdate(JsonString);
//}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body)
{
    return PlayerAttributeParser::DeserializePlayerStatsUpdate(Body);
}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	FPlayerStatsUpdateStruct PlayerStatsUpdate;

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		if (JsonObject->HasField(TEXT("body")))
		{
			TSharedPtr<FJsonObject> Body = JsonObject->GetObjectField(TEXT("body"));
			PlayerStatsUpdate = DeserializePlayerStatsUpdate(Body);
		}
	}

	return PlayerStatsUpdate;
}

// Player skills system parsers
FPlayerSkillNetworkData JSONParser::DeserializePlayerSkillNetworkData(const TSharedPtr<FJsonObject>& SkillObj)
{
	FPlayerSkillNetworkData SkillData;

	if (SkillObj.IsValid())
	{
		if (SkillObj->HasField(TEXT("skillSlug")))
		{
			SkillData.skillSlug = SkillObj->GetStringField(TEXT("skillSlug"));
		}

		if (SkillObj->HasField(TEXT("skillLevel")))
		{
			SkillData.skillLevel = SkillObj->GetIntegerField(TEXT("skillLevel"));
		}

		if (SkillObj->HasField(TEXT("castMs")))
		{
			SkillData.castMs = SkillObj->GetIntegerField(TEXT("castMs"));
		}

		if (SkillObj->HasField(TEXT("coeff")))
		{
			SkillData.coeff = SkillObj->GetNumberField(TEXT("coeff"));
		}

		if (SkillObj->HasField(TEXT("cooldownMs")))
		{
			SkillData.cooldownMs = SkillObj->GetIntegerField(TEXT("cooldownMs"));
		}

		if (SkillObj->HasField(TEXT("costMp")))
		{
			SkillData.costMp = SkillObj->GetIntegerField(TEXT("costMp"));
		}

		if (SkillObj->HasField(TEXT("flatAdd")))
		{
			SkillData.flatAdd = SkillObj->GetNumberField(TEXT("flatAdd"));
		}

		if (SkillObj->HasField(TEXT("gcdMs")))
		{
			SkillData.gcdMs = SkillObj->GetIntegerField(TEXT("gcdMs"));
		}

		if (SkillObj->HasField(TEXT("maxRange")))
		{
			SkillData.maxRange = SkillObj->GetNumberField(TEXT("maxRange"));
		}
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
		if (Body->HasField(TEXT("characterId")))
		{
			InitData.characterId = Body->GetIntegerField(TEXT("characterId"));
		}

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

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		if (JsonObject->HasField(TEXT("body")))
		{
			TSharedPtr<FJsonObject> Body = JsonObject->GetObjectField(TEXT("body"));
			InitData = DeserializePlayerSkillsInitialization(Body);
		}
	}

	return InitData;
}

