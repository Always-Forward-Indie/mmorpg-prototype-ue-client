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

		EffectObj->TryGetStringField(TEXT("effectName"), Effect.effectName);
	
		EffectObj->TryGetNumberField(TEXT("duration"), Effect.duration);
	
		EffectObj->TryGetNumberField(TEXT("value"), Effect.value);
	
		EffectObj->TryGetStringField(TEXT("effectType"), Effect.effectType);

	return Effect;
}

TArray<FAppliedEffectData> JSONParser::DeserializeAppliedEffects(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FAppliedEffectData> Effects;
	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		if (!Value.IsValid()) continue;
		if (Value->Type == EJson::String)
		{
			// skillExecutionResult format: appliedEffects is an array of slug strings.
			// Checked BEFORE any object conversion: AsObject() on a string
			// logs a LogJson Error per effect.
			FAppliedEffectData Effect;
			Value->TryGetString(Effect.effectName);
			Effects.Add(Effect);
			continue;
		}
		// TryGetObject (not AsObject): wrong-typed entries skip quietly.
		const TSharedPtr<FJsonObject>* EffectPtr = nullptr;
		if (!Value->TryGetObject(EffectPtr) || !EffectPtr || !(*EffectPtr).IsValid())
		{
			continue;
		}
		Effects.Add(JSONParser::DeserializeAppliedEffect(*EffectPtr));
	}
	return Effects;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const TSharedPtr<FJsonObject>& InitiationObj)
{
	FSkillInitiationData SkillData;
	if (!InitiationObj.IsValid()) return SkillData;

		InitiationObj->TryGetStringField(TEXT("skillName"), SkillData.skillName);

		InitiationObj->TryGetStringField(TEXT("skillSlug"), SkillData.skillSlug);
	
		InitiationObj->TryGetStringField(TEXT("animationName"), SkillData.animationName);
	
		InitiationObj->TryGetNumberField(TEXT("animationDuration"), SkillData.animationDuration);
	
		InitiationObj->TryGetNumberField(TEXT("castTime"), SkillData.castTime);
	
		InitiationObj->TryGetNumberField(TEXT("casterId"), SkillData.casterId);
	
		InitiationObj->TryGetNumberField(TEXT("casterType"), SkillData.casterType);
	
		InitiationObj->TryGetStringField(TEXT("casterTypeString"), SkillData.casterTypeString);
	
		InitiationObj->TryGetNumberField(TEXT("targetId"), SkillData.targetId);
	
		InitiationObj->TryGetNumberField(TEXT("targetType"), SkillData.targetType);
	
		InitiationObj->TryGetStringField(TEXT("targetTypeString"), SkillData.targetTypeString);
	
		InitiationObj->TryGetBoolField(TEXT("success"), SkillData.success);

	// Parse effect type and school
	FString EffectTypeStr;
	if (InitiationObj->TryGetStringField(TEXT("skillEffectType"), EffectTypeStr))
	{
		SkillData.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}

	FString SchoolStr;
	if (InitiationObj->TryGetStringField(TEXT("skillSchool"), SchoolStr))
	{
		SkillData.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

		InitiationObj->TryGetNumberField(TEXT("cooldownMs"), SkillData.cooldownMs);

		InitiationObj->TryGetNumberField(TEXT("gcdMs"), SkillData.gcdMs);

	double ServerTsMs = 0.0;
	if (InitiationObj->TryGetNumberField(TEXT("serverTimestamp"), ServerTsMs))
		SkillData.serverTimestamp = static_cast<int64>(ServerTsMs);

	double CastStartedMs = 0.0;
	if (InitiationObj->TryGetNumberField(TEXT("castStartedAt"), CastStartedMs))
		SkillData.castStartedAt = static_cast<int64>(CastStartedMs);

		InitiationObj->TryGetStringField(TEXT("errorReason"), SkillData.errorReason);
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
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeSkillInitiation failed to parse JSON"));
		FSkillInitiationData FailedInit; FailedInit.success = false; return FailedInit;
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeSkillInitiation missing body"));
		FSkillInitiationData FailedInit; FailedInit.success = false; FailedInit.errorCode = JSONParser::ResolveErrorCode(Root); FailedInit.errorReason = FailedInit.errorCode; return FailedInit;
	}
	TSharedPtr<FJsonObject> Body = *BodyPtr;

	TSharedPtr<FJsonObject> SkillInitiation; const TSharedPtr<FJsonObject>* SkillInitPtr = nullptr; if (Body->TryGetObjectField(TEXT("skillInitiation"), SkillInitPtr) && SkillInitPtr) SkillInitiation = *SkillInitPtr;
	if (!SkillInitiation.IsValid())
		{ FSkillInitiationData FailedInit; FailedInit.success = false; FailedInit.errorCode = JSONParser::ResolveErrorCode(Root); FailedInit.errorReason = FailedInit.errorCode; return FailedInit; }

	return JSONParser::DeserializeSkillInitiation(SkillInitiation);
}

FSkillResultData JSONParser::DeserializeSkillResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FSkillResultData SkillResult;
	if (!ResultObj.IsValid()) return SkillResult;

		ResultObj->TryGetStringField(TEXT("skillName"), SkillResult.skillName);
	
		ResultObj->TryGetStringField(TEXT("skillSlug"), SkillResult.skillSlug);
	
		ResultObj->TryGetNumberField(TEXT("casterId"), SkillResult.casterId);
	
		ResultObj->TryGetNumberField(TEXT("casterType"), SkillResult.casterType);
	
		ResultObj->TryGetStringField(TEXT("casterTypeString"), SkillResult.casterTypeString);
	
		ResultObj->TryGetNumberField(TEXT("targetId"), SkillResult.targetId);
	
		ResultObj->TryGetNumberField(TEXT("targetType"), SkillResult.targetType);
	
		ResultObj->TryGetStringField(TEXT("targetTypeString"), SkillResult.targetTypeString);
	
		ResultObj->TryGetNumberField(TEXT("damage"), SkillResult.damage);

		if (!ResultObj->TryGetNumberField(TEXT("healing"), SkillResult.healing))
			ResultObj->TryGetNumberField(TEXT("healAmount"), SkillResult.healing);

		ResultObj->TryGetNumberField(TEXT("manaHealing"), SkillResult.manaHealing);
	
		ResultObj->TryGetNumberField(TEXT("finalTargetHealth"), SkillResult.finalTargetHealth);
	
		ResultObj->TryGetNumberField(TEXT("finalTargetMana"), SkillResult.finalTargetMana);
	
		ResultObj->TryGetNumberField(TEXT("finalCasterMana"), SkillResult.finalCasterMana);
	
		ResultObj->TryGetBoolField(TEXT("isCritical"), SkillResult.isCritical);
	
		ResultObj->TryGetBoolField(TEXT("isBlocked"), SkillResult.isBlocked);
	
		ResultObj->TryGetBoolField(TEXT("isMissed"), SkillResult.isMissed);
	
		ResultObj->TryGetBoolField(TEXT("targetDied"), SkillResult.targetDied);
	
		ResultObj->TryGetBoolField(TEXT("success"), SkillResult.success);

	double ResultServerTsMs = 0.0;
	if (ResultObj->TryGetNumberField(TEXT("serverTimestamp"), ResultServerTsMs))
		SkillResult.serverTimestamp = static_cast<int64>(ResultServerTsMs);

		ResultObj->TryGetStringField(TEXT("errorReason"), SkillResult.errorReason);

	// Parse effect type and school
	FString ResultEffectTypeStr;
	if (ResultObj->TryGetStringField(TEXT("skillEffectType"), ResultEffectTypeStr))
	{
		SkillResult.skillEffectType = JSONParser::ParseSkillEffectType(ResultEffectTypeStr);
	}

	FString ResultSchoolStr;
	if (ResultObj->TryGetStringField(TEXT("skillSchool"), ResultSchoolStr))
	{
		SkillResult.skillSchool = JSONParser::ParseSkillSchool(ResultSchoolStr);
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
// TryGetObject (not AsObject): wrong-typed entries skip quietly.
const TSharedPtr<FJsonObject>* OPtr = nullptr;
if (!V.IsValid() || !V->TryGetObject(OPtr) || !OPtr || !(*OPtr).IsValid()) continue;
const TSharedPtr<FJsonObject>& O = *OPtr;
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
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeSkillResult failed to parse JSON"));
		FSkillResultData FailedRes; FailedRes.success = false; return FailedRes;
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeSkillResult missing body"));
		FSkillResultData FailedRes; FailedRes.success = false; FailedRes.errorCode = JSONParser::ResolveErrorCode(Root); FailedRes.errorReason = FailedRes.errorCode; return FailedRes;
	}
	TSharedPtr<FJsonObject> Body = *BodyPtr;

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
const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
TSharedPtr<FJsonObject> Header;
if (Root->TryGetObjectField(TEXT("header"), HeaderPtr) && HeaderPtr && (*HeaderPtr).IsValid()) Header = *HeaderPtr;
if (Header.IsValid())
{
	FString Status;
	if (Header->TryGetStringField(TEXT("status"), Status))
	{
		Result.success = Status.Equals(TEXT("success"), ESearchCase::IgnoreCase);
	}
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
