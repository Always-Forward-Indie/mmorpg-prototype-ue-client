#pragma once

#include "CoreMinimal.h"

/**
 * Result status normalization (pure logic, FString in / FString+bool out).
 *
 * Single owner of the success/error discriminator for result packets.
 * The wire shape is awkward: success packets carry header.status="success"
 * and NO message/code; error packets carry status="error" and/or a code
 * (body.errorCode wins, header.message is the fallback — see
 * JSONParser::ResolveErrorCode). Keying parsers off header.message treats
 * every success as an error with an empty code and zeroes all totals
 * (the vendor +0-gold blackout). Handlers must feed parsers the output of
 * ResolveMessage, never the raw header.message. Pinned by MMO.Protocol.ResultStatus.
 */
struct PROTOTYPING_API FResultStatusRule
{
	static bool IsError(const FString& Status, const FString& ResolvedCode)
	{
		return Status.Equals(TEXT("error"), ESearchCase::IgnoreCase)
			|| (!ResolvedCode.IsEmpty() && !ResolvedCode.Equals(TEXT("success"), ESearchCase::IgnoreCase));
	}

	/** "success" for results, else the error code (status as last resort). */
	static FString ResolveMessage(const FString& Status, const FString& ResolvedCode)
	{
		if (!IsError(Status, ResolvedCode)) return TEXT("success");
		return !ResolvedCode.IsEmpty() ? ResolvedCode : Status;
	}
};
