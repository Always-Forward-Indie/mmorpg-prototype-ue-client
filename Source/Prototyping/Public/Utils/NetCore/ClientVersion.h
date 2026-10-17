#pragma once

#include "CoreMinimal.h"

/**
 * Client version (SemVer MAJOR.MINOR.PATCH) helpers.
 *
 * Canonical implementation for the version sent as body.clientVersion on
 * login/register and gated server-side against [min_client_version,
 * max_client_version]. Pure and dependency-free so Automation specs can
 * call it directly. No UWorld, no UObject.
 */
struct PROTOTYPING_API FClientVersion
{
	/** Strict MAJOR.MINOR.PATCH with numeric parts (no 'v' prefix). */
	static bool IsSemVer(const FString& Version);

	/**
	 * Numeric SemVer comparison. @return -1 if A < B, 0 if equal, +1 if A > B.
	 * Returns 0 and sets bOutValid=false when either side is not SemVer.
	 */
	static int32 Compare(const FString& A, const FString& B, bool& bOutValid);

	/**
	 * Inclusive range check for the server compat gate: Min <= Version <= Max.
	 * False when any input is not SemVer (never gate on garbage).
	 */
	static bool IsCompatible(const FString& Version, const FString& MinVersion, const FString& MaxVersion);
};
