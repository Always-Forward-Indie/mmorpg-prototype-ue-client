#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// Protocol regression suite for the post-0.2.55/0.2.21/0.1.30 server contract:
// central body.errorCode, combatAoeResult batches, initiation error codes,
// explicit-zero durability. All functions under test are pure statics (no UWorld).

namespace
{
bool TryParseBody(const FString& Packet, TSharedPtr<FJsonObject>& OutBody)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Packet);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return false;
    }
    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr)
    {
        return false;
    }
    OutBody = *BodyPtr;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageDataRobustnessTest, "MMO.Protocol.MessageDataRobustness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMessageDataRobustnessTest::RunTest(const FString& Parameters)
{
    // Live crash (2026-10-17, chunk snapshot burst): a truncated mid-body
    // packet AV'd on a null JsonObject. Any unparseable input must yield
    // quiet defaults — hot per-packet path, no error-level log by design
    // (an unexpected Error would fail the harness by itself).
    FMessageDataStruct Garbage = JSONParser::DeserializeMessageData(TEXT("not json"));
    TestTrue(TEXT("garbage error empty"), Garbage.errorCode.IsEmpty());
    TestTrue(TEXT("garbage event empty"), Garbage.eventType.IsEmpty());

    FMessageDataStruct Empty = JSONParser::DeserializeMessageData(TEXT(""));
    TestTrue(TEXT("empty error empty"), Empty.errorCode.IsEmpty());

    FMessageDataStruct Truncated = JSONParser::DeserializeMessageData(
        TEXT("{\"header\":{\"eventType\":\"spawnSnapshot\",\"status\":\"success\"},\"body\":{\"mobs\":[{\"id\":1"));
    TestEqual(TEXT("truncated event default"), Truncated.eventType, FString());
    TestTrue(TEXT("truncated error empty"), Truncated.errorCode.IsEmpty());

    FMessageDataStruct NoBody = JSONParser::DeserializeMessageData(TEXT("{\"header\":{\"eventType\":\"pingClient\"}}"));
    TestEqual(TEXT("nobody event kept"), NoBody.eventType, FString(TEXT("pingClient")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResolveErrorCodeTest, "MMO.Protocol.ResolveErrorCode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FResolveErrorCodeTest::RunTest(const FString& Parameters)
{
    // body.errorCode wins over header.message.
    TestEqual(TEXT("body code wins"),
        JSONParser::ResolveErrorCode(
            TEXT("{\"header\":{\"eventType\":\"buyItem\",\"status\":\"error\",\"message\":\"human text\"},\"body\":{\"errorCode\":\"INSUFFICIENT_GOLD\"}}")),
        FString(TEXT("INSUFFICIENT_GOLD")));
    // Legacy packets without body.errorCode fall back to header.message.
    TestEqual(TEXT("header fallback"),
        JSONParser::ResolveErrorCode(
            TEXT("{\"header\":{\"eventType\":\"buyItem\",\"status\":\"error\",\"message\":\"OUT_OF_STOCK\"},\"body\":{}}")),
        FString(TEXT("OUT_OF_STOCK")));
    // Garbage in, empty out (never match logic on this).
    TestTrue(TEXT("garbage is empty"),
        JSONParser::ResolveErrorCode(TEXT("not json")).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageDataErrorFieldsTest, "MMO.Protocol.MessageDataErrorFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMessageDataErrorFieldsTest::RunTest(const FString& Parameters)
{
    // Login version reject carries machine code + range hints.
    FMessageDataStruct VersionReject = JSONParser::DeserializeMessageData(
        TEXT("{\"header\":{\"eventType\":\"authentificationClient\",\"status\":\"error\",\"message\":\"ERR_VERSION_OUTDATED\"},\"body\":{\"errorCode\":\"ERR_VERSION_OUTDATED\",\"serverVersion\":\"1.0\",\"minClientVersion\":\"0.1.2\",\"maxClientVersion\":\"0.1.4\"}}"));
    TestEqual(TEXT("version code"), VersionReject.errorCode, FString(TEXT("ERR_VERSION_OUTDATED")));
    TestEqual(TEXT("min hint"), VersionReject.minClientVersion, FString(TEXT("0.1.2")));
    TestEqual(TEXT("max hint"), VersionReject.maxClientVersion, FString(TEXT("0.1.4")));
    TestEqual(TEXT("server version"), VersionReject.serverVersion, FString(TEXT("1.0")));

    // Ban carries reason + expiry ("" = permanent).
    FMessageDataStruct Ban = JSONParser::DeserializeMessageData(
        TEXT("{\"header\":{\"eventType\":\"authentificationClient\",\"status\":\"error\",\"message\":\"ERR_ACCOUNT_BANNED\"},\"body\":{\"errorCode\":\"ERR_ACCOUNT_BANNED\",\"banReason\":\"griefing\",\"banExpiresAt\":\"\"}}"));
    TestEqual(TEXT("ban code"), Ban.errorCode, FString(TEXT("ERR_ACCOUNT_BANNED")));
    TestEqual(TEXT("ban reason"), Ban.banReason, FString(TEXT("griefing")));

    // Success packets report the success message as code (callers check status first).
    FMessageDataStruct Ok = JSONParser::DeserializeMessageData(
        TEXT("{\"header\":{\"eventType\":\"pingClient\",\"status\":\"success\",\"message\":\"Pong!\"},\"body\":{}}"));
    TestEqual(TEXT("success mirrors message"), Ok.errorCode, FString(TEXT("Pong!")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAoEResultBatchTest, "MMO.Protocol.AoEResultBatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAoEResultBatchTest::RunTest(const FString& Parameters)
{
    FSkillResultData Batch = JSONParser::DeserializeSkillResult(
        TEXT("{\"header\":{\"eventType\":\"combatAoeResult\",\"status\":\"success\",\"message\":\"ok\"},\"body\":{"
        "\"skillSlug\":\"fireball\",\"skillName\":\"Fireball\",\"casterId\":7,\"targetId\":101,"
        "\"damage\":50,\"isCritical\":false,\"isBlocked\":false,\"isMissed\":false,\"targetDied\":false,"
        "\"finalTargetHealth\":30,\"success\":true,"
        "\"aoeResult\":{\"targets\":["
        "{\"targetId\":101,\"damage\":50,\"isCritical\":false,\"isBlocked\":false,\"isMissed\":false,\"targetDied\":false,\"finalTargetHealth\":30},"
        "{\"targetId\":102,\"damage\":43,\"isCritical\":true,\"isBlocked\":false,\"isMissed\":false,\"targetDied\":false,\"finalTargetHealth\":12},"
        "{\"targetId\":103,\"damage\":0,\"isCritical\":false,\"isBlocked\":false,\"isMissed\":true,\"targetDied\":false,\"finalTargetHealth\":60}"
        "]}}}"));
    TestEqual(TEXT("three targets parsed"), Batch.aoeTargets.Num(), 3);
    if (Batch.aoeTargets.Num() == 3)
    {
        TestEqual(TEXT("second target id"), Batch.aoeTargets[1].targetId, 102);
        TestEqual(TEXT("second target damage"), Batch.aoeTargets[1].damage, 43);
        TestTrue(TEXT("second target crit"), Batch.aoeTargets[1].isCritical);
        TestTrue(TEXT("third target missed"), Batch.aoeTargets[2].isMissed);
    }

    // Single-target results carry no batch array.
    FSkillResultData Single = JSONParser::DeserializeSkillResult(
        TEXT("{\"header\":{\"eventType\":\"combatResult\",\"status\":\"success\",\"message\":\"ok\"},\"body\":{"
        "\"skillResult\":{\"skillSlug\":\"strike\",\"casterId\":7,\"targetId\":101,\"damage\":10,\"success\":true}}}"));
    TestEqual(TEXT("single has no batch"), Single.aoeTargets.Num(), 0);
    TestEqual(TEXT("single damage"), Single.damage, 10);

    // Failed results keep the machine code for UI branches.
    FSkillResultData Failed = JSONParser::DeserializeSkillResult(
        TEXT("{\"header\":{\"eventType\":\"combatResult\",\"status\":\"error\",\"message\":\"OUT_OF_RANGE\"},\"body\":{\"errorCode\":\"OUT_OF_RANGE\",\"success\":false}}"));
    TestEqual(TEXT("failed keeps code"), Failed.errorCode, FString(TEXT("OUT_OF_RANGE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillInitiationErrorTest, "MMO.Protocol.SkillInitiationError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillInitiationErrorTest::RunTest(const FString& Parameters)
{
    // CC rejection arrives as status:error with body.errorCode and no skillInitiation object.
    FSkillInitiationData Stunned = JSONParser::DeserializeSkillInitiation(
        TEXT("{\"header\":{\"eventType\":\"combatInitiation\",\"status\":\"error\",\"message\":\"Stunned\"},\"body\":{\"errorCode\":\"Stunned\"}}"));
    TestFalse(TEXT("stunned fails"), Stunned.success);
    TestEqual(TEXT("stun code"), Stunned.errorCode, FString(TEXT("Stunned")));

    // Error without body.errorCode falls back to header.message.
    FSkillInitiationData Legacy = JSONParser::DeserializeSkillInitiation(
        TEXT("{\"header\":{\"eventType\":\"combatInitiation\",\"status\":\"error\",\"message\":\"Silenced\"},\"body\":{}}"));
    TestFalse(TEXT("legacy fails"), Legacy.success);
    TestEqual(TEXT("silence fallback"), Legacy.errorCode, FString(TEXT("Silenced")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDurabilityZeroKeptTest, "MMO.Protocol.DurabilityZeroKept",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDurabilityZeroKeptTest::RunTest(const FString& Parameters)
{
    // Explicit 0 from the server means broken — never mask it with max.
    TSharedPtr<FJsonObject> BrokenObj;
    TestTrue(TEXT("broken parses"), TryParseBody(
        TEXT("{\"header\":{},\"body\":{\"id\":5,\"itemId\":10,\"quantity\":1,\"durabilityCurrent\":0,\"durabilityMax\":100,\"isDurable\":true}}"),
        BrokenObj));
    FInventoryItemStruct Broken = JSONParser::DeserializeInventoryItem(BrokenObj);
    TestEqual(TEXT("explicit zero kept"), Broken.durabilityCurrent, 0);

    // Missing field still falls back to max.
    TSharedPtr<FJsonObject> LegacyObj;
    TestTrue(TEXT("legacy parses"), TryParseBody(
        TEXT("{\"header\":{},\"body\":{\"id\":5,\"itemId\":10,\"quantity\":1,\"durabilityMax\":100,\"isDurable\":true}}"),
        LegacyObj));
    FInventoryItemStruct Legacy = JSONParser::DeserializeInventoryItem(LegacyObj);
    TestEqual(TEXT("missing falls back to max"), Legacy.durabilityCurrent, 100);
    return true;
}
