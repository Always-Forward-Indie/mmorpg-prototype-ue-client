#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "Networking/NetworkDispatcher.h"

// Single-parse packet contract (S05): ParsePacket deserializes once and
// shares Root/Body/EventType. Garbage and headerless payloads yield an
// invalid packet (quiet defaults, never a crash). Calls production
// JSONParser::ParsePacket.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPacketParseTest, "MMO.Protocol.Packet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPacketParseTest::RunTest(const FString& Parameters)
{
    {
        const FParsedPacket Packet = JSONParser::ParsePacket(
            TEXT("{\"header\":{\"eventType\":\"tradeInvite\",\"status\":\"success\"},\"body\":{\"sessionId\":\"abc\"}}"));
        TestTrue(TEXT("valid packet"), Packet.bValid);
        TestEqual(TEXT("event type"), Packet.EventType, TEXT("tradeInvite"));
        TestEqual(TEXT("status"), Packet.Status, TEXT("success"));
        TestTrue(TEXT("root set"), Packet.Root.IsValid());
        TestTrue(TEXT("body set"), Packet.Body.IsValid());
        FString Session;
        TestTrue(TEXT("body readable"), Packet.Body->TryGetStringField(TEXT("sessionId"), Session));
        TestEqual(TEXT("body value"), Session, TEXT("abc"));
    }
    {
        const FParsedPacket NoBody = JSONParser::ParsePacket(
            TEXT("{\"header\":{\"eventType\":\"pingClient\"}}"));
        TestTrue(TEXT("no-body valid"), NoBody.bValid);
        TestFalse(TEXT("no-body null"), NoBody.Body.IsValid());
    }
    TestFalse(TEXT("garbage invalid"), JSONParser::ParsePacket(TEXT("not json")).bValid);
    TestFalse(TEXT("empty invalid"), JSONParser::ParsePacket(TEXT("")).bValid);
    TestFalse(TEXT("no header invalid"),
        JSONParser::ParsePacket(TEXT("{\"body\":{\"a\":1}}")).bValid);
    TestFalse(TEXT("empty eventType invalid"),
        JSONParser::ParsePacket(TEXT("{\"header\":{\"status\":\"success\"}}")).bValid);
    {
        // Legacy quirk (chunk characterExperience): header.event instead of
        // header.eventType. Routes as valid; eventType wins when both present.
        const FParsedPacket Quirk = JSONParser::ParsePacket(
            TEXT("{\"header\":{\"event\":\"characterExperience\",\"status\":\"success\"},\"body\":{}}"));
        TestTrue(TEXT("event-key quirk valid"), Quirk.bValid);
        TestEqual(TEXT("event-key quirk type"), Quirk.EventType, TEXT("characterExperience"));
        TestEqual(TEXT("event-key quirk status"), Quirk.Status, TEXT("success"));
        const FParsedPacket Both = JSONParser::ParsePacket(
            TEXT("{\"header\":{\"eventType\":\"a\",\"event\":\"b\"}}"));
        TestEqual(TEXT("eventType wins over event"), Both.EventType, TEXT("a"));
    }

    // Re-parsing the same Raw twice yields equal results (deterministic).
    const FString Raw = TEXT("{\"header\":{\"eventType\":\"moveCharacter\"},\"body\":{}}");
    TestEqual(TEXT("deterministic event"),
        JSONParser::ParsePacket(Raw).EventType,
        JSONParser::ParsePacket(Raw).EventType);
    return true;
}

// Dispatcher routing contract (S05): exact eventType delivery, unknown and
// invalid packets dispatch to nobody, fallbacks fire for every valid packet.
// Calls production FNetworkDispatcher.

struct FDispatchProbe
{
    int32 Calls = 0;
    void OnPacket(const FParsedPacket& Packet) { ++Calls; LastEvent = Packet.EventType; }
    FString LastEvent;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDispatchRouteTest, "MMO.Protocol.Dispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDispatchRouteTest::RunTest(const FString& Parameters)
{
    FNetworkDispatcher Dispatcher;
    TestEqual(TEXT("empty registry"), Dispatcher.NumHandledEventTypes(), 0);

    FDispatchProbe TradeProbe;
    FDispatchProbe FallbackProbe;
    Dispatcher.AddHandler(TEXT("tradeInvite"), &TradeProbe, &FDispatchProbe::OnPacket);
    Dispatcher.AddFallbackHandler(&FallbackProbe, &FDispatchProbe::OnPacket);
    TestEqual(TEXT("one route"), Dispatcher.NumHandledEventTypes(), 1);

    const FParsedPacket Trade = JSONParser::ParsePacket(
        TEXT("{\"header\":{\"eventType\":\"tradeInvite\"},\"body\":{}}"));
    TestEqual(TEXT("routed count"), Dispatcher.Dispatch(Trade), 2);
    TestEqual(TEXT("handler fired"), TradeProbe.Calls, 1);
    TestEqual(TEXT("handler event"), TradeProbe.LastEvent, TEXT("tradeInvite"));
    TestEqual(TEXT("fallback fired"), FallbackProbe.Calls, 1);

    const FParsedPacket Other = JSONParser::ParsePacket(
        TEXT("{\"header\":{\"eventType\":\"moveCharacter\"},\"body\":{}}"));
    TestEqual(TEXT("unrouted count"), Dispatcher.Dispatch(Other), 1);
    TestEqual(TEXT("handler skipped"), TradeProbe.Calls, 1);
    TestEqual(TEXT("fallback fired again"), FallbackProbe.Calls, 2);

    const FParsedPacket Garbage = JSONParser::ParsePacket(TEXT("###"));
    TestEqual(TEXT("invalid dispatched nowhere"), Dispatcher.Dispatch(Garbage), 0);
    TestEqual(TEXT("fallback skipped on invalid"), FallbackProbe.Calls, 2);

    Dispatcher.RemoveHandler(TEXT("tradeInvite"), &TradeProbe);
    TestEqual(TEXT("after remove"), Dispatcher.Dispatch(Trade), 1);
    TestEqual(TEXT("handler removed"), TradeProbe.Calls, 1);
    return true;
}
