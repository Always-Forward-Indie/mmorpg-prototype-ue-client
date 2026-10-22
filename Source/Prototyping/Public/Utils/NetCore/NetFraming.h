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

	/**
	 * Byte offsets of one `wireFrag` frame body. Pure so specs can pin it.
	 * No key-order assumption: number fields are trusted only OUTSIDE the
	 * payload span (a payload may quote "total":N text of its own — and
	 * live senders emit keys alphabetically, numbers after payload).
	 */
	struct FWireFragFramePos
	{
		uint64 StreamId = 0;
		uint64 Index = 0;
		uint64 Total = 0;
		/** Payload value bytes (without quotes), may split UTF-8. */
		int32 PayloadBegin = -1;
		int32 PayloadLen = 0;
	};

	/**
	 * Parse one complete wireFrag line (WITHOUT the trailing delimiter).
	 * Key order is NOT assumed (live senders emit alphabetically: body
	 * before header, numbers after payload). OutPos points at the RAW
	 * payload span (still JSON-escaped — decode with UnescapeJsonString
	 * before feeding the reassembler, mirroring the server which parses
	 * frames with a real JSON parser).
	 * Returns false on any shape violation (caller swallows the line —
	 * the sender's totals can no longer be trusted for this stream).
	 */
	static bool ParseWireFragFrame(const uint8* Data, int32 Len, FWireFragFramePos& OutPos);

	/**
	 * Decode a JSON string value body (no surrounding quotes): \" \\ \/
	 * \b \f \n \r \t and \uXXXX (with surrogate pairs, emitted as UTF-8).
	 * Raw bytes (incl. split-nothing — this runs on one complete frame)
	 * pass through untouched. Returns false on truncated/invalid escapes.
	 */
	static bool UnescapeJsonString(const uint8* Data, int32 Len, TArray<uint8>& OutBytes);

	/**
	 * Raw-line twin of ExtractPackets: same split/drop/buffer rules, but
	 * segments stay byte arrays (no UTF-8 decode) so the caller can route
	 * wireFrag frames to the reassembler before decoding. Oversized
	 * segments are dropped with the same error log; the cap still applies:
	 * frames are small by construction, anything bigger is a violation.
	 */
	static int32 ExtractRawLines(TArray<uint8>& InOutAccumulatedBytes, TArray<TArray<uint8>>& OutLines,
	const TCHAR* SocketTag = nullptr);
};
