#pragma once

#include "CoreMinimal.h"

/**
 * Whisper echo policy (pure logic, FString in / bool out).
 *
 * The server delivers whispers to the target only — the sender gets no echo
 * (only a "not found" error). Zone/local come back via broadcast, so only
 * whispers need an optimistic local echo to appear in the sender's chat.
 * Echoing anything else would double every line. Pinned by MMO.Chat.WhisperEcho.
 */
struct PROTOTYPING_API FChatWhisperRule
{
	static bool NeedsLocalEcho(const FString& Channel)
	{
		return Channel.Equals(TEXT("whisper"), ESearchCase::IgnoreCase);
	}
};
