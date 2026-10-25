#pragma once

#include "CoreMinimal.h"

/**
 * Receiver-side reassembly for `wireFrag` transport frames.
 *
 * Server contract (chunk JsonLineChunking.hpp, issues #121/#122): oversize
 * logical messages arrive as small `wireFrag` lines; the receiver groups
 * them by streamId and concatenates payloads in index order. Order does not
 * matter, duplicates are ignored, a mismatched total resets the stream.
 *
 * Byte-oriented on purpose: the server slices the message by BYTES, so a
 * multi-byte UTF-8 codepoint may straddle two frames. Accumulating decoded
 * FStrings would corrupt it (per-line decode replaces the halves); raw
 * bytes concatenate exactly and decode once at completion.
 *
 * Pure, thread-safe for single-owner use (one receiver worker owns one
 * instance), dependency-free (no UWorld, no sockets) so Automation specs
 * can pin it. Time is injected (NowSec) — no clock inside, deterministic
 * in tests.
 */
struct PROTOTYPING_API FWireFragReassembler
{
	/** Max payload bytes accepted per frame (server budget + headroom). */
	static constexpr int32 MaxPayloadBytes = 6 * 1024 + 1024;
	/** Sanity cap for one logical message (DoS guard, mirrors server). */
	static constexpr int32 MaxLogicalBytes = 1024 * 1024;
	/** Max frames per logical message (mirrors server). */
	static constexpr uint64 MaxFramesPerMessage = 1024;
	/** Max concurrent in-flight streams per connection (mirrors server). */
	static constexpr int32 MaxStreams = 16;
	/** Idle-stream reap threshold in seconds (mirrors server). */
	static constexpr double DefaultReapTtlSec = 30.0;

	struct FFeedResult
	{
		/** Last needed frame arrived; Message holds the raw logical bytes. */
		bool bComplete = false;
		/** Frame rejected (limits/shape); the stream was reset. */
		bool bDropped = false;
		/** Completed logical message bytes (no trailing delimiter). */
		TArray<uint8> Message;
	};

	/**
	 * Feed one frame body. Out-of-order and duplicate frames are fine;
	 * mismatched totals or over-limit shapes reset the stream.
	 * @param NowSec caller clock seconds (e.g. FPlatformTime::Seconds()).
	 */
	FFeedResult Feed(uint64 StreamId, uint64 Index, uint64 Total,
		const uint8* Payload, int32 PayloadLen, double NowSec);

	/** Drop streams idle longer than TtlSec. @return remaining streams. */
	int32 ReapIdle(double NowSec, double TtlSec = DefaultReapTtlSec);

	int32 PendingStreams() const { return Streams.Num(); }

private:
	struct FStream
	{
		uint64 Total = 0;
		TArray<TArray<uint8>> Parts;
		TArray<uint8> Filled;
		int32 Have = 0;
		double LastUpdateSec = 0.0;
	};
	TMap<uint64, FStream> Streams;
};

/**
 * Reassembly observability (S13-C3): success must be as loud as failure.
 * Drops already log Error inside FNetFraming; completions were silent.
 * Pure counters so Automation specs can pin the transitions; the single
 * owner is the receiver worker (game thread reads them only after the
 * worker thread joined in TeardownLink — no atomics needed).
 */
struct PROTOTYPING_API FWireFragStats
{
	uint64 FramesSeen = 0;
	uint64 StreamsCompleted = 0;
	uint64 FramesMalformed = 0;
	uint64 BytesReassembled = 0;

	/** A wireFrag line arrived and parsed (completing or not). */
	void OnFrameFed() { ++FramesSeen; }
	/** A wireFrag line failed parse/unescape or violated limits. */
	void OnFrameMalformed() { ++FramesMalformed; }
	/** A stream completed: frames were already counted in FramesSeen. */
	void OnStreamCompleted(int32 ReassembledBytes) { ++StreamsCompleted; BytesReassembled += static_cast<uint64>(ReassembledBytes); }
};
