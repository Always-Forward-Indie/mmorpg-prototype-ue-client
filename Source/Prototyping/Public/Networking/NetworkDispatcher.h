#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Single-parse network packet (S05).
 *
 * Parsed ONCE in UNetworkManager::PollLink via JSONParser::ParsePacket and
 * shared with the time-sync path and the per-eventType dispatcher, so the
 * ~30 subscribed handlers no longer re-parse every packet just to filter
 * their eventType. Plain struct (no UCLASS/USTRUCT — TSharedPtr members
 * are not UPROPERTY-compatible). Passed by const reference.
 */
struct PROTOTYPING_API FParsedPacket
{
	/** header.eventType, empty when the packet has no usable header. */
	FString EventType;
	/** header.status ("success"/"error"), empty when absent. */
	FString Status;
	/** header.message (server text, also the error fallback). */
	FString Message;
	/** Full parsed root (null when the payload is not valid JSON). */
	TSharedPtr<FJsonObject> Root;
	/** body object (null when the packet carries no body). */
	TSharedPtr<FJsonObject> Body;
	/** Original wire text (kept for compat broadcasts and error logs). */
	FString Raw;
	/** True when Root parsed and EventType is non-empty. */
	bool bValid = false;
};

/** One routed subscription. No owner = always live (tests / free functions). */
struct PROTOTYPING_API FPacketHandlerEntry
{
	TWeakObjectPtr<UObject> Owner;
	TFunction<void(const FParsedPacket&)> Fn;
	bool bHasOwner = false;

	bool IsLive() const { return !bHasOwner || Owner.IsValid(); }
};

/**
 * Minimal eventType router (S05).
 *
 * Handlers register per eventType (or as a fallback for every packet) in
 * SubscribeToNetworkEvents and unregister in UnsubscribeFromNetworkEvents —
 * same discipline as the legacy FString delegates. Stale UObject owners are
 * skipped via weak pointers, so a missed unregister degrades to a no-op
 * instead of a crash.
 */
struct PROTOTYPING_API FNetworkDispatcher
{
	/** Register Obj::Method for one eventType. */
	template <typename T>
	void AddHandler(const FString& EventType, T* Obj, void (T::*Method)(const FParsedPacket&))
	{
		FPacketHandlerEntry Entry;
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			Entry.Owner = Obj;
			Entry.bHasOwner = true;
		}
		Entry.Fn = [Obj, Method](const FParsedPacket& Packet) { (Obj->*Method)(Packet); };
		FindOrAdd(EventType).Add(MoveTemp(Entry));
	}

	/** Unregister all of Obj's callbacks for one eventType. */
	template <typename T>
	void RemoveHandler(const FString& EventType, T* Obj)
	{
		UObject* AsUObject = nullptr;
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			AsUObject = Obj;
		}
		if (TArray<FPacketHandlerEntry>* Found = Handlers.Find(EventType))
		{
			Found->RemoveAll([AsUObject](const FPacketHandlerEntry& Entry) { return Entry.Owner.Get() == AsUObject; });
		}
	}

	/** Register Obj::Method as a fallback fired for every valid packet. */
	template <typename T>
	void AddFallbackHandler(T* Obj, void (T::*Method)(const FParsedPacket&))
	{
		FPacketHandlerEntry Entry;
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			Entry.Owner = Obj;
			Entry.bHasOwner = true;
		}
		Entry.Fn = [Obj, Method](const FParsedPacket& Packet) { (Obj->*Method)(Packet); };
		Fallbacks.Add(MoveTemp(Entry));
	}

	/** Unregister all of Obj's fallback callbacks. */
	template <typename T>
	void RemoveFallbackHandler(T* Obj)
	{
		UObject* AsUObject = nullptr;
		if constexpr (std::is_base_of_v<UObject, T>)
		{
			AsUObject = Obj;
		}
		Fallbacks.RemoveAll([AsUObject](const FPacketHandlerEntry& Entry) { return Entry.Owner.Get() == AsUObject; });
	}

	/** Route one parsed packet. Returns the number of handlers fired. */
	int32 Dispatch(const FParsedPacket& Packet) const;

	/** Number of eventTypes with at least one handler (spec pin). */
	int32 NumHandledEventTypes() const { return Handlers.Num(); }

private:
	TArray<FPacketHandlerEntry>& FindOrAdd(const FString& EventType);

	TMap<FString, TArray<FPacketHandlerEntry>> Handlers;
	TArray<FPacketHandlerEntry> Fallbacks;
};
