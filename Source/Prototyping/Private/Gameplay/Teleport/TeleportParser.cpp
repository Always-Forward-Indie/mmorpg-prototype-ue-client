#include "Gameplay/Teleport/TeleportParser.h"
#include "Prototyping.h"

bool FTeleportParser::ParseRespawnResult(const TSharedPtr<FJsonObject>& Body, const FString& RequestIdEcho, FTeleportTargetData& Out, int32 ResolvedCharacterId)
{
	Out = FTeleportTargetData();
	Out.RequestIdEcho = RequestIdEcho;

	if (!Body.IsValid())
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportParser: respawnResult body is null"));
		return false;
	}

	Body->TryGetNumberField(TEXT("characterId"), Out.CharacterId);
	if (Out.CharacterId <= 0)
	{
		Out.CharacterId = ResolvedCharacterId;
	}
	if (Out.CharacterId <= 0)
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportParser: respawnResult missing characterId"));
		return false;
	}

	const TSharedPtr<FJsonObject>* PosObj = nullptr;
	if (!Body->TryGetObjectField(TEXT("position"), PosObj) || PosObj == nullptr || !(*PosObj).IsValid())
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportParser: respawnResult missing position for charId=%d"), Out.CharacterId);
		return false;
	}

	(*PosObj)->TryGetNumberField(TEXT("x"), Out.X);
	(*PosObj)->TryGetNumberField(TEXT("y"), Out.Y);
	(*PosObj)->TryGetNumberField(TEXT("z"), Out.Z);
	double RawYaw = 0.0;
	(*PosObj)->TryGetNumberField(TEXT("rotationZ"), RawYaw);
	Out.Yaw = static_cast<float>(RawYaw);

	const TSharedPtr<FJsonObject>* HealthObj = nullptr;
	const TSharedPtr<FJsonObject>* ManaObj = nullptr;
	int32 HP = -1, MaxHP = -1, MP = -1, MaxMP = -1;
	if (Body->TryGetObjectField(TEXT("health"), HealthObj) && HealthObj && (*HealthObj).IsValid())
	{
		(*HealthObj)->TryGetNumberField(TEXT("current"), HP);
		(*HealthObj)->TryGetNumberField(TEXT("max"), MaxHP);
	}
	if (Body->TryGetObjectField(TEXT("mana"), ManaObj) && ManaObj && (*ManaObj).IsValid())
	{
		(*ManaObj)->TryGetNumberField(TEXT("current"), MP);
		(*ManaObj)->TryGetNumberField(TEXT("max"), MaxMP);
	}
	if (HP >= 0 && MaxHP > 0 && MP >= 0 && MaxMP > 0)
	{
		Out.bHasVitals = true;
		Out.HealthCurrent = HP;
		Out.HealthMax = MaxHP;
		Out.ManaCurrent = MP;
		Out.ManaMax = MaxMP;
	}

	return true;
}

bool FTeleportParser::ParsePlayerTeleported(const TSharedPtr<FJsonObject>& Body, FTeleportTargetData& Out)
{
	Out = FTeleportTargetData();

	if (!Body.IsValid())
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportParser: playerTeleported body is null"));
		return false;
	}

	Body->TryGetNumberField(TEXT("characterId"), Out.CharacterId);
	if (Out.CharacterId <= 0)
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportParser: playerTeleported missing characterId"));
		return false;
	}

	bool bHasX = Body->TryGetNumberField(TEXT("x"), Out.X);
	bool bHasY = Body->TryGetNumberField(TEXT("y"), Out.Y);
	bool bHasZ = Body->TryGetNumberField(TEXT("z"), Out.Z);
	double RawYaw = 0.0;
	Body->TryGetNumberField(TEXT("rotZ"), RawYaw);
	Out.Yaw = static_cast<float>(RawYaw);

	if (!bHasX || !bHasY || !bHasZ)
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportParser: playerTeleported missing coords for charId=%d"), Out.CharacterId);
		return false;
	}

	return true;
}
