#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "TeleportParser.generated.h"

/**
 * Authoritative teleport target parsed from a server packet (plain data).
 *
 * Produced by FTeleportParser from respawnResult / playerTeleported bodies
 * and consumed by UTeleportManager. Vitals ride along only on respawnResult
 * (skill teleports and admin teleports carry none — bHasVitals is false).
 */
USTRUCT(BlueprintType)
struct PROTOTYPING_API FTeleportTargetData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	int32 CharacterId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	double X = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	double Y = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	double Z = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	float Yaw = 0.0f;

	/** True when the packet carried authoritative HP/MP (respawnResult). */
	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	bool bHasVitals = false;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	int32 HealthCurrent = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	int32 HealthMax = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	int32 ManaCurrent = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	int32 ManaMax = -1;

	/** requestIdEcho binding this result to its request (empty = predates echo). */
	UPROPERTY(BlueprintReadOnly, Category = "Teleport")
	FString RequestIdEcho;
};

/**
 * Pure packet parsers for teleport-class events (no UObject, no UWorld).
 *
 * Single owner of the respawnResult / playerTeleported wire contract:
 * - respawnResult body: {characterId, position{x,y,z,rotationZ},
 *   health{current,max}?, mana{current,max}?}
 * - playerTeleported body: {characterId, x, y, z, rotZ}
 *
 * Every seam failure logs error-level (never silent — watch_logs SEAM
 * counters treat these like FATAL). Automation contract specs pin both
 * shapes via ParserContractHelpers (engine-only JSON, no mirrored logic).
 */
struct PROTOTYPING_API FTeleportParser
{
	/** Parse a respawnResult body. False when characterId/position is unusable.
	 *  ResolvedCharacterId covers legacy servers that omit characterId
	 *  (resolved by the caller from the connected-player map). */
	static bool ParseRespawnResult(const TSharedPtr<FJsonObject>& Body, const FString& RequestIdEcho, FTeleportTargetData& Out, int32 ResolvedCharacterId = 0);

	/** Parse a playerTeleported body. False when characterId/coords are unusable. */
	static bool ParsePlayerTeleported(const TSharedPtr<FJsonObject>& Body, FTeleportTargetData& Out);
};
