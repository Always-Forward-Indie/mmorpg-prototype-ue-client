#pragma once

#include "CoreMinimal.h"

/**
 * Canonical framing for the custom TCP+JSON protocol, v2 (S14, chunk
 * docs/api/14-framing-v2-protocol.md): [u32 LE length][UTF-8 JSON].
 *
 * The v1 line protocol ('\n'-terminated, 8KB cap, wireFrag reassembly) is
 * deleted — do not reintroduce per-event chunking or line caps. Messages of
 * any size up to MaxFrameBytes pass through as one frame; the transport
 * never drops by size, it errors loudly (seam rule).
 *
 * Pure, thread-safe, dependency-free (FString/TArray only, no UWorld,
 * no sockets) so both the socket workers and Automation specs can call it.
 */
struct PROTOTYPING_API FNetFraming
{
	/** Max single frame body size in bytes (spec cap, mirrors servers). */
	static constexpr int32 MaxFrameBytes = 4 * 1024 * 1024;

	static bool IsOversized(int32 ByteCount) { return ByteCount > MaxFrameBytes; }

	/**
	 * Byte-level framed extractor for NetworkReceiverWorker's accumulated
	 * socket buffer. Consumes complete [len][json] frames, leaves the
	 * partial tail buffered. UTF-8 is decoded only for complete messages,
	 * so split multi-byte sequences across Recv() chunks stay intact.
	 * No delimiter scanning: the length IS the boundary.
	 *
	 * Spec enforcement (error-level: seam failure points are never silent):
	 * a length prefix above MaxFrameBytes clears the buffer and returns -1
	 * (fatal for the connection — the caller breaks the read loop so the
	 * drop/reconnect machinery restores a clean stream). Truncated tails
	 * simply wait for more bytes.
	 * @return number of complete messages, or -1 on fatal framing violation.
	 */
	static int32 ExtractFramedMessages(TArray<uint8>& InOutAccumulatedBytes, TArray<FString>& OutMessages,
	const TCHAR* SocketTag = nullptr);

	/** Little-endian u32 load (framing is defined LE on the wire). */
	static uint32 ReadU32LE(const uint8* Data)
	{
		return static_cast<uint32>(Data[0])
			| (static_cast<uint32>(Data[1]) << 8)
			| (static_cast<uint32>(Data[2]) << 16)
			| (static_cast<uint32>(Data[3]) << 24);
	}
};
