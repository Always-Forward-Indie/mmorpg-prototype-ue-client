#pragma once

#include "CoreMinimal.h"

/**
 * Throwaway character names for one-shot live scenarios (pure logic).
 *
 * Canonical owner of the Qabot-name predicate used by the live batch
 * (quest/level run on a fresh char instead of the fixed 614/615 pair):
 * one-shot content (non-repeatable quest 9000, exp_for_level cap at 10)
 * needs a clean char per run. Names must satisfy the login-server contract
 * (letters + single spaces only, 2..20 chars, no leading/trailing or double
 * spaces — digits/underscores are rejected), so the timestamp is encoded as
 * base-26 letters (A-Z), never digits.
 */
struct PROTOTYPING_API FThrowawayNameRule
{
	/** Prefix for all throwaway names (with the single separating space). */
	static FString Prefix() { return TEXT("Qabot "); }

	/** Suffix length: 4 letters = 456k combos per prefix (enough with time seed). */
	static int32 SuffixLen() { return 4; }

	/** Encodes Seed as SuffixLen() base-26 uppercase letters (A-Z). */
	static FString EncodeSuffix(uint32 Seed)
	{
		FString Out;
		Out.Reserve(SuffixLen());
		uint32 V = Seed;
		for (int32 i = 0; i < SuffixLen(); ++i)
		{
			Out.AppendChar(TCHAR('A' + (V % 26)));
			V /= 26;
		}
		return Out;
	}

	/** Full throwaway name: "Qabot XXXX" (10 chars, always valid). */
	static FString MakeQabotName(uint32 Seed)
	{
		return Prefix() + EncodeSuffix(Seed);
	}

	/** True when Name is one of ours (prefix + letters only). */
	static bool IsQabotName(const FString& Name)
	{
		const FString P = Prefix();
		if (!Name.StartsWith(P)) { return false; }
		if (Name.Len() != P.Len() + SuffixLen()) { return false; }
		for (int32 i = P.Len(); i < Name.Len(); ++i)
		{
			const TCHAR C = Name[i];
			if (C < TCHAR('A') || C > TCHAR('Z')) { return false; }
		}
		return true;
	}

	/** Mirrors the login-server name contract (letters/spaces/apostrophes). */
	static bool IsValidCharName(const FString& Name)
	{
		const int32 N = Name.Len();
		if (N < 2 || N > 20) { return false; }
		if (Name[0] == TCHAR(' ') || Name[N - 1] == TCHAR(' ')) { return false; }
		bool bPrevSpace = false;
		for (int32 i = 0; i < N; ++i)
		{
			const TCHAR C = Name[i];
			const bool bLetter = (C >= TCHAR('A') && C <= TCHAR('Z')) || (C >= TCHAR('a') && C <= TCHAR('z'));
			const bool bSpace = (C == TCHAR(' '));
			const bool bApos = (C == TCHAR('\''));
			if (!bLetter && !bSpace && !bApos) { return false; }
			if (bSpace)
			{
				if (bPrevSpace) { return false; }
				bPrevSpace = true;
			}
			else { bPrevSpace = false; }
		}
		return true;
	}
};
