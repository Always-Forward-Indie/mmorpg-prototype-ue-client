// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// Combat data deserializers
FCombatAnimationData JSONParser::DeserializeCombatAnimation(const TSharedPtr<FJsonObject>& AnimationObj)
{
	FCombatAnimationData AnimationData;
	if (!AnimationObj.IsValid()) return AnimationData;

	if (AnimationObj->HasField(TEXT("animationName")))
		AnimationData.AnimationName = AnimationObj->GetStringField(TEXT("animationName"));
	
	if (AnimationObj->HasField(TEXT("characterId")))
		AnimationData.CharacterId = AnimationObj->GetIntegerField(TEXT("characterId"));
	
	if (AnimationObj->HasField(TEXT("duration")))
		AnimationData.Duration = AnimationObj->GetNumberField(TEXT("duration"));
	
	if (AnimationObj->HasField(TEXT("isLooping")))
		AnimationData.bIsLooping = AnimationObj->GetBoolField(TEXT("isLooping"));

	return AnimationData;
}

FCombatActionData JSONParser::DeserializeCombatAction(const TSharedPtr<FJsonObject>& ActionObj)
{
	FCombatActionData ActionData;
	if (!ActionObj.IsValid()) return ActionData;

	if (ActionObj->HasField(TEXT("actionId")))
		ActionData.ActionId = ActionObj->GetIntegerField(TEXT("actionId"));
	
	if (ActionObj->HasField(TEXT("actionName")))
		ActionData.ActionName = ActionObj->GetStringField(TEXT("actionName"));
	
	if (ActionObj->HasField(TEXT("actionType")))
		ActionData.ActionType = ActionObj->GetIntegerField(TEXT("actionType"));
	
	if (ActionObj->HasField(TEXT("casterId")))
		ActionData.CasterId = ActionObj->GetIntegerField(TEXT("casterId"));
	
	if (ActionObj->HasField(TEXT("targetId")))
		ActionData.TargetId = ActionObj->GetIntegerField(TEXT("targetId"));
	
	if (ActionObj->HasField(TEXT("targetType")))
		ActionData.TargetType = ActionObj->GetIntegerField(TEXT("targetType"));
	
	if (ActionObj->HasField(TEXT("targetTypeString")))
		ActionData.TargetTypeString = ActionObj->GetStringField(TEXT("targetTypeString"));

	return ActionData;
}

FCombatResultData JSONParser::DeserializeCombatResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FCombatResultData ResultData;
	if (!ResultObj.IsValid()) return ResultData;

	if (ResultObj->HasField(TEXT("actionId")))
		ResultData.ActionId = ResultObj->GetIntegerField(TEXT("actionId"));
	
	if (ResultObj->HasField(TEXT("casterId")))
		ResultData.CasterId = ResultObj->GetIntegerField(TEXT("casterId"));
	
	if (ResultObj->HasField(TEXT("damageDealt")))
		ResultData.DamageDealt = ResultObj->GetIntegerField(TEXT("damageDealt"));
	
	if (ResultObj->HasField(TEXT("healingDone")))
		ResultData.HealingDone = ResultObj->GetIntegerField(TEXT("healingDone"));
	
	if (ResultObj->HasField(TEXT("isBlocked")))
		ResultData.bIsBlocked = ResultObj->GetBoolField(TEXT("isBlocked"));
	
	if (ResultObj->HasField(TEXT("isCritical")))
		ResultData.bIsCritical = ResultObj->GetBoolField(TEXT("isCritical"));
	
	if (ResultObj->HasField(TEXT("isDodged")))
		ResultData.bIsDodged = ResultObj->GetBoolField(TEXT("isDodged"));
	
	if (ResultObj->HasField(TEXT("isResisted")))
		ResultData.bIsResisted = ResultObj->GetBoolField(TEXT("isResisted"));
	
	if (ResultObj->HasField(TEXT("remainingHealth")))
		ResultData.RemainingHealth = ResultObj->GetIntegerField(TEXT("remainingHealth"));
	
	if (ResultObj->HasField(TEXT("remainingMana")))
		ResultData.RemainingMana = ResultObj->GetIntegerField(TEXT("remainingMana"));
	
	if (ResultObj->HasField(TEXT("targetDied")))
		ResultData.bTargetDied = ResultObj->GetBoolField(TEXT("targetDied"));
	
	if (ResultObj->HasField(TEXT("isDamaged")))
		ResultData.bIsDamaged = ResultObj->GetBoolField(TEXT("isDamaged"));
	
	if (ResultObj->HasField(TEXT("targetId")))
		ResultData.TargetId = ResultObj->GetIntegerField(TEXT("targetId"));
	
	if (ResultObj->HasField(TEXT("targetType")))
		ResultData.TargetType = ResultObj->GetIntegerField(TEXT("targetType"));
	
	if (ResultObj->HasField(TEXT("targetTypeString")))
		ResultData.TargetTypeString = ResultObj->GetStringField(TEXT("targetTypeString"));

	return ResultData;
}

