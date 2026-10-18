#pragma once

#include "CoreMinimal.h"

/**
 * Canonical framing for the custom TCP+JSON protocol.
 *
 * Spec (see MMO.Protocol.Framing test): one JSON object per line, '\n'
 * terminated, empty/whitespace-only lines ignored, 8KB max per message.
 * Trailing bytes without a delimiter are a partial message and stay buffered.
 *
 * Pure, thread-safe, dependency-free (FString/TArray only) so both the
 * socket workers and Automation specs can call it. No UWorld, no sockets.
 */
struct PROTOTYPING_API FNetFraming
{
	/** Max single message size in bytes (spec cap). */
	static constexpr int32 MaxMessageBytes = 8 * 1024;

	static bool IsOversized(int32 ByteCount) { return ByteCount > MaxMessageBytes; }

	/**
	 * FString-level split. Splits InBuffer on '\n', trims each segment,
	 * skips empty ones. The trailing segment without a delimiter is
	 * returned as OutRest (partial message, kept for the next chunk).
	 * @return number of complete messages in OutMessages.
	 */
	static int32 SplitFraming(const FString& InBuffer, TArray<FString>& OutMessages, FString& OutRest);

	/**
	 * Byte-level extractor for NetworkReceiverWorker's accumulated socket
	 * buffer. Splits on '\n' BEFORE UTF-8 decoding so multi-byte sequences
	 * split across Recv() chunks stay intact. Consumes complete packets from
	 * InOutAccumulatedBytes, leaves the partial tail buffered.
	 *
	 * Spec enforcement (error-level: seam failure points are never silent):
	 * - oversized segments (> MaxMessageBytes) are dropped with an error log;
	 * - a delimiter-less tail past the cap is dropped with an error log
	 *   so a hung sender cannot grow the buffer unbounded.
	 * @return number of complete messages in OutMessages.
	 */
	static int32 ExtractPackets(TArray<uint8>& InOutAccumulatedBytes, TArray<FString>& OutMessages,
	const TCHAR* SocketTag = nullptr);

	/**
	 * Best-effort eventType peek for drop logs (seam observability).
	 * Scans the first MaxScanBytes for `"eventType"` followed by optional
	 * whitespace, `:`, optional whitespace and a quoted string value
	 * (tolerates `"eventType" : "x"` spacing variants). Byte-level, no UTF-8
	 * decoding — safe on truncated/binary input. Returns "?" when absent.
	 * Pure so specs can pin it.
	 */
	static FString PeekEventType(const uint8* Data, int32 Len, int32 MaxScanBytes = 256);

	/**
	 * Sanitized printable prefix of a byte buffer (for drop logs).
	 * Non-printable bytes become '?'. Pure so specs can pin it.
	 */
	static FString PeekHead(const uint8* Data, int32 Len, int32 MaxChars = 96);
};
