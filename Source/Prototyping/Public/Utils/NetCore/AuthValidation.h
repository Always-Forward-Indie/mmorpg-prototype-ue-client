#pragma once

#include "CoreMinimal.h"

/** Machine-readable outcome of a client-side credential length check. */
enum class EAuthValueError : uint8
{
	Ok,
	Empty,
	TooShort,
	TooLong
};

/**
 * Client-side credential validation (pure logic, no UObject).
 *
 * Canonical owner of the length rules mirrored from the server contract:
 * login 3..20 chars, password 8..100 chars. UAuthenticationManager keeps
 * the user-facing side effects (log + broadcast text) and maps this enum
 * onto them 1:1 — behavior is unchanged, only the predicate is shared
 * and pinned by MMO.Auth.Validation.
 */
struct PROTOTYPING_API FAuthValidation
{
	static EAuthValueError ValidateLogin(const FString& Login)
	{
		if (Login.IsEmpty()) { return EAuthValueError::Empty; }
		if (Login.Len() < 3) { return EAuthValueError::TooShort; }
		if (Login.Len() > 20) { return EAuthValueError::TooLong; }
		return EAuthValueError::Ok;
	}

	static EAuthValueError ValidatePassword(const FString& Password)
	{
		if (Password.IsEmpty()) { return EAuthValueError::Empty; }
		// Server expects 8–100 chars, not 3–20.
		if (Password.Len() < 8) { return EAuthValueError::TooShort; }
		if (Password.Len() > 100) { return EAuthValueError::TooLong; }
		return EAuthValueError::Ok;
	}

	static bool IsOk(EAuthValueError Error) { return Error == EAuthValueError::Ok; }
};
