// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// New combat system parsing functions
ESkillEffectType JSONParser::ParseSkillEffectType(const FString& EffectTypeString)
{
	if (EffectTypeString.Equals(TEXT("damage"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Damage;
	else if (EffectTypeString.Equals(TEXT("heal"), ESearchCase::IgnoreCase)
		  || EffectTypeString.Equals(TEXT("healing"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Healing;
	else if (EffectTypeString.Equals(TEXT("buff"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Buff;
	else if (EffectTypeString.Equals(TEXT("debuff"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Debuff;
	else if (EffectTypeString.Equals(TEXT("resource"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Resource;
	
	return ESkillEffectType::None;
}

ESkillSchool JSONParser::ParseSkillSchool(const FString& SchoolString)
{
	if (SchoolString.Equals(TEXT("physical"), ESearchCase::IgnoreCase))
		return ESkillSchool::Physical;
	else if (SchoolString.Equals(TEXT("fire"), ESearchCase::IgnoreCase))
		return ESkillSchool::Fire;
	else if (SchoolString.Equals(TEXT("ice"), ESearchCase::IgnoreCase))
		return ESkillSchool::Ice;
	else if (SchoolString.Equals(TEXT("nature"), ESearchCase::IgnoreCase))
		return ESkillSchool::Nature;
	else if (SchoolString.Equals(TEXT("arcane"), ESearchCase::IgnoreCase))
		return ESkillSchool::Arcane;
	else if (SchoolString.Equals(TEXT("shadow"), ESearchCase::IgnoreCase))
		return ESkillSchool::Shadow;
	else if (SchoolString.Equals(TEXT("holy"), ESearchCase::IgnoreCase)
		  || SchoolString.Equals(TEXT("healing"), ESearchCase::IgnoreCase))
		return ESkillSchool::Holy;

	return ESkillSchool::None;
}
ECasterType JSONParser::ParseCasterType(const FString& CasterTypeString)
{
	if (CasterTypeString.Equals(TEXT("SELF"), ESearchCase::IgnoreCase))
		return ECasterType::Self;
	if (CasterTypeString.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase) ||
		CasterTypeString.Equals(TEXT("Player"), ESearchCase::IgnoreCase))
		return ECasterType::Player;
	if (CasterTypeString.Equals(TEXT("MOB"), ESearchCase::IgnoreCase) ||
		CasterTypeString.Equals(TEXT("Mob"), ESearchCase::IgnoreCase))
		return ECasterType::Mob;
	if (CasterTypeString.Equals(TEXT("AREA"), ESearchCase::IgnoreCase))
		return ECasterType::Area;
	if (CasterTypeString.Equals(TEXT("NPC"), ESearchCase::IgnoreCase) ||
		CasterTypeString.Equals(TEXT("Npc"), ESearchCase::IgnoreCase))
		return ECasterType::NPC;
	
	return ECasterType::None;
}

FAppliedEffectData JSONParser::DeserializeAppliedEffect(const TSharedPtr<FJsonObject>& EffectObj)
{
	FAppliedEffectData Effect;
	if (!EffectObj.IsValid()) return Effect;

	if (EffectObj->HasField(TEXT("effectName")))
		Effect.effectName = EffectObj->GetStringField(TEXT("effectName"));
	
	if (EffectObj->HasField(TEXT("duration")))
		Effect.duration = EffectObj->GetNumberField(TEXT("duration"));
	
	if (EffectObj->HasField(TEXT("value")))
		Effect.value = EffectObj->GetIntegerField(TEXT("value"));
	
	if (EffectObj->HasField(TEXT("effectType")))
		Effect.effectType = EffectObj->GetStringField(TEXT("effectType"));

	return Effect;
}

TArray<FAppliedEffectData> JSONParser::DeserializeAppliedEffects(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FAppliedEffectData> Effects;
	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		const TSharedPtr<FJsonObject> EffectObj = Value->AsObject();
		if (EffectObj.IsValid())
		{
			Effects.Add(JSONParser::DeserializeAppliedEffect(EffectObj));
		}
		else if (Value->Type == EJson::String)
		{
			// skillExecutionResult format: appliedEffects is an array of slug strings
			FAppliedEffectData Effect;
			Effect.effectName = Value->AsString();
			Effects.Add(Effect);
		}
	}
	return Effects;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const TSharedPtr<FJsonObject>& InitiationObj)
{
	FSkillInitiationData SkillData;
	if (!InitiationObj.IsValid()) return SkillData;

	if (InitiationObj->HasField(TEXT("skillName")))
		SkillData.skillName = InitiationObj->GetStringField(TEXT("skillName"));

	if (InitiationObj->HasField(TEXT("skillSlug")))
		SkillData.skillSlug = InitiationObj->GetStringField(TEXT("skillSlug"));
	
	if (InitiationObj->HasField(TEXT("animationName")))
		SkillData.animationName = InitiationObj->GetStringField(TEXT("animationName"));
	
	if (InitiationObj->HasField(TEXT("animationDuration")))
		SkillData.animationDuration = InitiationObj->GetNumberField(TEXT("animationDuration"));
	
	if (InitiationObj->HasField(TEXT("castTime")))
		SkillData.castTime = InitiationObj->GetNumberField(TEXT("castTime"));
	
	if (InitiationObj->HasField(TEXT("casterId")))
		SkillData.casterId = InitiationObj->GetIntegerField(TEXT("casterId"));
	
	if (InitiationObj->HasField(TEXT("casterType")))
		SkillData.casterType = InitiationObj->GetIntegerField(TEXT("casterType"));
	
	if (InitiationObj->HasField(TEXT("casterTypeString")))
		SkillData.casterTypeString = InitiationObj->GetStringField(TEXT("casterTypeString"));
	
	if (InitiationObj->HasField(TEXT("targetId")))
		SkillData.targetId = InitiationObj->GetIntegerField(TEXT("targetId"));
	
	if (InitiationObj->HasField(TEXT("targetType")))
		SkillData.targetType = InitiationObj->GetIntegerField(TEXT("targetType"));
	
	if (InitiationObj->HasField(TEXT("targetTypeString")))
		SkillData.targetTypeString = InitiationObj->GetStringField(TEXT("targetTypeString"));
	
	if (InitiationObj->HasField(TEXT("success")))
		SkillData.success = InitiationObj->GetBoolField(TEXT("success"));

	// Parse effect type and school
	if (InitiationObj->HasField(TEXT("skillEffectType")))
	{
		FString EffectTypeStr = InitiationObj->GetStringField(TEXT("skillEffectType"));
		SkillData.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}
	
	if (InitiationObj->HasField(TEXT("skillSchool")))
	{
		FString SchoolStr = InitiationObj->GetStringField(TEXT("skillSchool"));
		SkillData.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

	if (InitiationObj->HasField(TEXT("cooldownMs")))
		SkillData.cooldownMs = InitiationObj->GetIntegerField(TEXT("cooldownMs"));

	if (InitiationObj->HasField(TEXT("gcdMs")))
		SkillData.gcdMs = InitiationObj->GetIntegerField(TEXT("gcdMs"));

	if (InitiationObj->HasField(TEXT("serverTimestamp")))
		SkillData.serverTimestamp = static_cast<int64>(InitiationObj->GetNumberField(TEXT("serverTimestamp")));

	if (InitiationObj->HasField(TEXT("castStartedAt")))
		SkillData.castStartedAt = static_cast<int64>(InitiationObj->GetNumberField(TEXT("castStartedAt")));

	if (InitiationObj->HasField(TEXT("errorReason")))
		SkillData.errorReason = InitiationObj->GetStringField(TEXT("errorReason"));
if (InitiationObj->HasField(TEXT("errorCode")))
{
InitiationObj->TryGetStringField(TEXT("errorCode"), SkillData.errorCode);
}
else
{
SkillData.errorCode = SkillData.errorReason;
}

	return SkillData;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{ FSkillInitiationData FailedInit; FailedInit.success = false; FailedInit.errorCode = JSONParser::ResolveErrorCode(Root); FailedInit.errorReason = FailedInit.errorCode; return FailedInit; }

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		{ FSkillInitiationData FailedInit; FailedInit.success = false; FailedInit.errorCode = JSONParser::ResolveErrorCode(Root); FailedInit.errorReason = FailedInit.errorCode; return FailedInit; }

	TSharedPtr<FJsonObject> SkillInitiation; const TSharedPtr<FJsonObject>* SkillInitPtr = nullptr; if (Body->TryGetObjectField(TEXT("skillInitiation"), SkillInitPtr) && SkillInitPtr) SkillInitiation = *SkillInitPtr;
	if (!SkillInitiation.IsValid())
		{ FSkillInitiationData FailedInit; FailedInit.success = false; FailedInit.errorCode = JSONParser::ResolveErrorCode(Root); FailedInit.errorReason = FailedInit.errorCode; return FailedInit; }

	return JSONParser::DeserializeSkillInitiation(SkillInitiation);
}

FSkillResultData JSONParser::DeserializeSkillResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FSkillResultData SkillResult;
	if (!ResultObj.IsValid()) return SkillResult;

	if (ResultObj->HasField(TEXT("skillName")))
		SkillResult.skillName = ResultObj->GetStringField(TEXT("skillName"));
	
	if (ResultObj->HasField(TEXT("skillSlug")))
		SkillResult.skillSlug = ResultObj->GetStringField(TEXT("skillSlug"));
	
	if (ResultObj->HasField(TEXT("casterId")))
		SkillResult.casterId = ResultObj->GetIntegerField(TEXT("casterId"));
	
	if (ResultObj->HasField(TEXT("casterType")))
		SkillResult.casterType = ResultObj->GetIntegerField(TEXT("casterType"));
	
	if (ResultObj->HasField(TEXT("casterTypeString")))
		SkillResult.casterTypeString = ResultObj->GetStringField(TEXT("casterTypeString"));
	
	if (ResultObj->HasField(TEXT("targetId")))
		SkillResult.targetId = ResultObj->GetIntegerField(TEXT("targetId"));
	
	if (ResultObj->HasField(TEXT("targetType")))
		SkillResult.targetType = ResultObj->GetIntegerField(TEXT("targetType"));
	
	if (ResultObj->HasField(TEXT("targetTypeString")))
		SkillResult.targetTypeString = ResultObj->GetStringField(TEXT("targetTypeString"));
	
	if (ResultObj->HasField(TEXT("damage")))
		SkillResult.damage = ResultObj->GetIntegerField(TEXT("damage"));
	
	if (ResultObj->HasField(TEXT("healing")))
		SkillResult.healing = ResultObj->GetIntegerField(TEXT("healing"));
	else if (ResultObj->HasField(TEXT("healAmount")))
		SkillResult.healing = ResultObj->GetIntegerField(TEXT("healAmount"));

	if (ResultObj->HasField(TEXT("manaHealing")))
		SkillResult.manaHealing = ResultObj->GetIntegerField(TEXT("manaHealing"));
	
	if (ResultObj->HasField(TEXT("finalTargetHealth")))
		SkillResult.finalTargetHealth = ResultObj->GetIntegerField(TEXT("finalTargetHealth"));
	
	if (ResultObj->HasField(TEXT("finalTargetMana")))
		SkillResult.finalTargetMana = ResultObj->GetIntegerField(TEXT("finalTargetMana"));
	
	if (ResultObj->HasField(TEXT("finalCasterMana")))
		SkillResult.finalCasterMana = ResultObj->GetIntegerField(TEXT("finalCasterMana"));
	
	if (ResultObj->HasField(TEXT("isCritical")))
		SkillResult.isCritical = ResultObj->GetBoolField(TEXT("isCritical"));
	
	if (ResultObj->HasField(TEXT("isBlocked")))
		SkillResult.isBlocked = ResultObj->GetBoolField(TEXT("isBlocked"));
	
	if (ResultObj->HasField(TEXT("isMissed")))
		SkillResult.isMissed = ResultObj->GetBoolField(TEXT("isMissed"));
	
	if (ResultObj->HasField(TEXT("targetDied")))
		SkillResult.targetDied = ResultObj->GetBoolField(TEXT("targetDied"));
	
	if (ResultObj->HasField(TEXT("success")))
		SkillResult.success = ResultObj->GetBoolField(TEXT("success"));

	if (ResultObj->HasField(TEXT("serverTimestamp")))
		SkillResult.serverTimestamp = static_cast<int64>(ResultObj->GetNumberField(TEXT("serverTimestamp")));

	if (ResultObj->HasField(TEXT("errorReason")))
		SkillResult.errorReason = ResultObj->GetStringField(TEXT("errorReason"));

	// Parse effect type and school
	if (ResultObj->HasField(TEXT("skillEffectType")))
	{
		FString EffectTypeStr = ResultObj->GetStringField(TEXT("skillEffectType"));
		SkillResult.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}
	
	if (ResultObj->HasField(TEXT("skillSchool")))
	{
		FString SchoolStr = ResultObj->GetStringField(TEXT("skillSchool"));
		SkillResult.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

	// Parse applied effects array
	if (ResultObj->HasField(TEXT("appliedEffects")))
	{
		const TArray<TSharedPtr<FJsonValue>>* EffectsArray;
		if (ResultObj->TryGetArrayField(TEXT("appliedEffects"), EffectsArray))
		{
			SkillResult.appliedEffects = JSONParser::DeserializeAppliedEffects(*EffectsArray);
		}
	}
if (ResultObj->HasField(TEXT("errorCode")))
{
ResultObj->TryGetStringField(TEXT("errorCode"), SkillResult.errorCode);
}
// combatAoeResult batch: body.aoeResult.targets[] (per-target damage/flags).
const TSharedPtr<FJsonObject>* AoeObj = nullptr;
if (ResultObj->TryGetObjectField(TEXT("aoeResult"), AoeObj) && AoeObj && *AoeObj)
{
const TArray<TSharedPtr<FJsonValue>>* TargetsArray = nullptr;
if ((*AoeObj)->TryGetArrayField(TEXT("targets"), TargetsArray) && TargetsArray)
{
for (const TSharedPtr<FJsonValue>& V : *TargetsArray)
{
TSharedPtr<FJsonObject> O = V->AsObject();
if (!O.IsValid()) continue;
FAoeTargetResultData T;
O->TryGetNumberField(TEXT("targetId"), T.targetId);
O->TryGetNumberField(TEXT("damage"), T.damage);
O->TryGetNumberField(TEXT("healing"), T.healing);
O->TryGetBoolField(TEXT("isCritical"), T.isCritical);
O->TryGetBoolField(TEXT("isBlocked"), T.isBlocked);
O->TryGetBoolField(TEXT("isMissed"), T.isMissed);
O->TryGetBoolField(TEXT("targetDied"), T.targetDied);
O->TryGetNumberField(TEXT("finalTargetHealth"), T.finalTargetHealth);
O->TryGetNumberField(TEXT("finalTargetMana"), T.finalTargetMana);
SkillResult.aoeTargets.Add(MoveTemp(T));
}
}
}

	return SkillResult;
}

FSkillResultData JSONParser::DeserializeSkillResult(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{ FSkillResultData FailedRes; FailedRes.success = false; FailedRes.errorCode = JSONParser::ResolveErrorCode(Root); FailedRes.errorReason = FailedRes.errorCode; return FailedRes; }

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		{ FSkillResultData FailedRes; FailedRes.success = false; FailedRes.errorCode = JSONParser::ResolveErrorCode(Root); FailedRes.errorReason = FailedRes.errorCode; return FailedRes; }

	// Try nested skillResult object first (combatResult / healingResult format)
	TSharedPtr<FJsonObject> SkillResult; const TSharedPtr<FJsonObject>* SkillResPtr = nullptr; if (Body->TryGetObjectField(TEXT("skillResult"), SkillResPtr) && SkillResPtr) SkillResult = *SkillResPtr;
	if (SkillResult.IsValid())
	{
		return JSONParser::DeserializeSkillResult(SkillResult);
	}

	// Fallback: fields directly in body (skillExecutionResult format)
	FSkillResultData Result = JSONParser::DeserializeSkillResult(Body);

	// skillExecutionResult wraps success in the header, not in body
if (!Body->HasField(TEXT("success")))
{
TSharedPtr<FJsonObject> Header = Root->GetObjectField(TEXT("header"));
if (Header.IsValid() && Header->HasField(TEXT("status")))
{
Result.success = Header->GetStringField(TEXT("status")).Equals(TEXT("success"), ESearchCase::IgnoreCase);
}
}
if (Result.errorCode.IsEmpty())
{
Result.errorCode = JSONParser::ResolveErrorCode(Root);
}

return Result;
}


FEffectTickData JSONParser::DeserializeEffectTick(const FString& JsonString)
{
    FEffectTickData TickData;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return TickData;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return TickData;

    const TSharedPtr<FJsonObject>& Body = *BodyPtr;
    Body->TryGetNumberField(TEXT("characterId"), TickData.characterId);
    Body->TryGetStringField(TEXT("effectSlug"), TickData.effectSlug);
    Body->TryGetStringField(TEXT("effectTypeSlug"), TickData.effectTypeSlug);
    Body->TryGetNumberField(TEXT("value"), TickData.value);
    Body->TryGetBoolField(TEXT("isHeal"), TickData.bIsHeal);
    Body->TryGetNumberField(TEXT("newHealth"), TickData.newHealth);
    Body->TryGetNumberField(TEXT("newMana"), TickData.newMana);
    Body->TryGetBoolField(TEXT("targetDied"), TickData.targetDied);

    return TickData;
}

TArray<FActiveEffectEntry> JSONParser::DeserializeActiveEffectsPacket(const FString& JsonString)
{
    TArray<FActiveEffectEntry> Effects;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Effects;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return Effects;

    const TArray<TSharedPtr<FJsonValue>>* EffectsArray = nullptr;
    if (!(*BodyPtr)->TryGetArrayField(TEXT("activeEffects"), EffectsArray)) return Effects;

    for (const TSharedPtr<FJsonValue>& Val : *EffectsArray)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Val->TryGetObject(ObjPtr)) continue;

        FActiveEffectEntry Entry;
        (*ObjPtr)->TryGetStringField(TEXT("slug"), Entry.slug);
        (*ObjPtr)->TryGetStringField(TEXT("effectType"), Entry.effectType);
        (*ObjPtr)->TryGetStringField(TEXT("effectTypeSlug"), Entry.effectTypeSlug);
        (*ObjPtr)->TryGetStringField(TEXT("attributeSlug"), Entry.attributeSlug);
        (*ObjPtr)->TryGetStringField(TEXT("sourceType"), Entry.sourceType);
        double Tmp = 0.0;
        if ((*ObjPtr)->TryGetNumberField(TEXT("value"), Tmp)) Entry.value = static_cast<float>(Tmp);
        if ((*ObjPtr)->TryGetNumberField(TEXT("expiresAt"), Tmp)) Entry.expiresAt = static_cast<int64>(Tmp);
        (*ObjPtr)->TryGetBoolField(TEXT("isPercentage"), Entry.bIsPercentage);
        (*ObjPtr)->TryGetBoolField(TEXT("isPermanent"), Entry.bIsPermanent);

        Effects.Add(Entry);
    }

    return Effects;
}

// ─────────────────────────────────────────────────────────────────────────────
// World Interactive Objects (WIO) parsers
