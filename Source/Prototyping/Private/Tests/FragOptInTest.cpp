#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// Wire contract (S13-C1, server twin: EventDispatcher checks
// context.json["body"]["frag"].is_boolean()): every chunk-bound join
// message carries {"frag":true} via the single JSONParser::AddFragOptIn
// helper — no forked inline copies in the senders. All cases call the
// production helper + serializer, then read back like the server does.

namespace
{
bool TryReadFragFlag(const FString& Packet, bool& OutVal)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Packet);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return false;
    }
    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
    {
        return false;
    }
    return (*BodyPtr)->TryGetBoolField(TEXT("frag"), OutVal);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFragOptInTest, "MMO.Protocol.FragOptIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FFragOptInTest::RunTest(const FString& Parameters)
{
    // Empty body + helper: frag present and boolean-true on the wire.
    {
        TMap<FString, TSharedPtr<FJsonValue>> Header;
        TMap<FString, TSharedPtr<FJsonValue>> Body;
        JSONParser::AddFragOptIn(Body);
        const FString Packet = JSONParser::SerializeJson(TEXT("joinGameClient"), Header, Body);
        bool bFrag = false;
        TestTrue(TEXT("frag readable as bool"), TryReadFragFlag(Packet, bFrag));
        TestTrue(TEXT("frag true"), bFrag);
    }
    // Additive: existing fields survive, helper only adds its key.
    {
        TMap<FString, TSharedPtr<FJsonValue>> Header;
        TMap<FString, TSharedPtr<FJsonValue>> Body;
        Body.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(615)));
        JSONParser::AddFragOptIn(Body);
        const FString Packet = JSONParser::SerializeJson(TEXT("joinGameCharacter"), Header, Body);
        bool bFrag = false;
        TestTrue(TEXT("frag kept with payload"), TryReadFragFlag(Packet, bFrag));
        TestTrue(TEXT("frag true with payload"), bFrag);
        TestTrue(TEXT("packet mentions character"), Packet.Contains(TEXT("615")));
    }
    // Idempotent: double call keeps a single true (Add overwrites).
    {
        TMap<FString, TSharedPtr<FJsonValue>> Body;
        JSONParser::AddFragOptIn(Body);
        JSONParser::AddFragOptIn(Body);
        TestEqual(TEXT("single frag key"), Body.Num(), 1);
        const FString Packet = JSONParser::SerializeJson(TEXT("playerReady"), TMap<FString, TSharedPtr<FJsonValue>>(), Body);
        bool bFrag = false;
        TestTrue(TEXT("frag true after double add"), TryReadFragFlag(Packet, bFrag) && bFrag);
    }
    return true;
}
