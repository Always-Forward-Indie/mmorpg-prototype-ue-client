#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "ParserContractHelpers.h"

// Contract specs for the NetSync/header entry points (roadmap P0.1).
// Deliberate exception to the packet-level error rule: DeserializeNetworkHeader
// runs on the per-packet hot path (every received packet via
// ProcessTimeSyncFromHeader), so malformed input yields quiet defaults — a
// per-packet Error would spam the log on any hiccup. Failures surface
// observably instead: the time-sync SampleRule rejects the sample. Message
// entries (DeserializeMessageData etc.) keep the error rule; see
// ProtocolErrorCodeTest. All functions under test are production
// JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNetworkHeaderContractTest, "MMO.Protocol.NetworkHeaderContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNetworkHeaderContractTest::RunTest(const FString& Parameters)
{
    FNetworkHeaderStruct Valid = JSONParser::DeserializeNetworkHeader(
        TEXT("{\"header\":{\"eventType\":\"pongClient\",\"status\":\"success\",\"clientId\":11,\"hash\":\"abc\","
        "\"requestIdEcho\":\"sync_1\",\"clientSendMs\":1000,\"serverRecvMs\":1010,\"serverSendMs\":1015,"
        "\"message\":\"ok\"}}"));
    TestEqual(TEXT("valid event"), Valid.eventType, FString(TEXT("pongClient")));
    TestEqual(TEXT("valid client"), Valid.clientId, 11);
    TestEqual(TEXT("valid req echo wins"), Valid.requestId, FString(TEXT("sync_1")));
    TestEqual(TEXT("valid recv"), Valid.serverRecvMs, static_cast<int64>(1010));
    TestEqual(TEXT("valid send"), Valid.serverSendMs, static_cast<int64>(1015));

    // requestId (no Echo) fallback.
    FNetworkHeaderStruct ReqOnly = JSONParser::DeserializeNetworkHeader(
        TEXT("{\"header\":{\"requestId\":\"sync_2\"}}"));
    TestEqual(TEXT("request fallback"), ReqOnly.requestId, FString(TEXT("sync_2")));

    // Root-level timestamps object overrides header fields (pongClient §1.4).
    FNetworkHeaderStruct Ts = JSONParser::DeserializeNetworkHeader(
        TEXT("{\"header\":{\"serverRecvMs\":1,\"serverSendMs\":2},"
        "\"timestamps\":{\"serverRecvMs\":2010,\"serverSendMs\":2015,\"clientSendMsEcho\":2000}}"));
    TestEqual(TEXT("ts recv wins"), Ts.serverRecvMs, static_cast<int64>(2010));
    TestEqual(TEXT("ts send wins"), Ts.serverSendMs, static_cast<int64>(2015));
    TestEqual(TEXT("ts echo"), Ts.clientSendMsEcho, static_cast<int64>(2000));

    // Garbage and headerless packets: quiet defaults (hot path, no log).
    FNetworkHeaderStruct Garbage = JSONParser::DeserializeNetworkHeader(TEXT("not json"));
    TestTrue(TEXT("garbage event empty"), Garbage.eventType.IsEmpty());
    FNetworkHeaderStruct NoHeader = JSONParser::DeserializeNetworkHeader(TEXT("{\"body\":{}}"));
    TestEqual(TEXT("noheader recv default"), NoHeader.serverRecvMs, static_cast<int64>(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClientDataContractTest, "MMO.Protocol.ClientDataContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FClientDataContractTest::RunTest(const FString& Parameters)
{
    FClientDataStruct Valid = JSONParser::DeserializeClientData(
        TEXT("{\"header\":{\"clientId\":11,\"hash\":\"abc123\"},\"body\":{}}"));
    TestEqual(TEXT("valid client"), Valid.clientId, 11);
    TestEqual(TEXT("valid hash"), Valid.hash, FString(TEXT("abc123")));

    // Missing fields and garbage: quiet defaults (login-flow entry).
    FClientDataStruct Short = JSONParser::DeserializeClientData(TEXT("{\"header\":{}}"));
    TestEqual(TEXT("short client default"), Short.clientId, 0);
    FClientDataStruct Garbage = JSONParser::DeserializeClientData(TEXT("not json"));
    TestEqual(TEXT("garbage client default"), Garbage.clientId, 0);
    TestTrue(TEXT("garbage hash empty"), Garbage.hash.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEventTypeContractTest, "MMO.Protocol.EventTypeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FEventTypeContractTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("valid event"), JSONParser::DeserializeEventTypeData(
        TEXT("{\"header\":{\"eventType\":\"combatResult\"},\"body\":{}}")), FString(TEXT("combatResult")));
    TestTrue(TEXT("missing empty"), JSONParser::DeserializeEventTypeData(TEXT("{\"header\":{}}")).IsEmpty());
    TestTrue(TEXT("garbage empty"), JSONParser::DeserializeEventTypeData(TEXT("not json")).IsEmpty());
    return true;
}
