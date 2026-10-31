#include "Misc/AutomationTest.h"
#include "Gameplay/Teleport/TeleportParser.h"
#include "Utils/NetCore/TeleportRule.h"
#include "ParserContractHelpers.h"

// Teleport wire contract: respawnResult (full + vitals-less skill shape +
// legacy missing characterId) and playerTeleported. Calls the production
// FTeleportParser — never mirrors it. Error paths expect the error-level
// seam logs or the harness fails the spec.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeleportParserTest, "MMO.Teleport.Parser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTeleportParserTest::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("TeleportParser"), EAutomationExpectedErrorFlags::Contains, 4);

    FTeleportTargetData Out;
    bool bOk = FTeleportParser::ParseRespawnResult(ParserContract_ParseObj(
        TEXT("{\"characterId\":7,\"position\":{\"x\":700,\"y\":-3200,\"z\":200,\"rotationZ\":90},")
        TEXT("\"health\":{\"current\":300,\"max\":1000},\"mana\":{\"current\":150,\"max\":500}}")),
        TEXT("sync_9"), Out);
    TestTrue(TEXT("respawn full parses"), bOk);
    TestEqual(TEXT("respawn char"), Out.CharacterId, 7);
    TestEqual(TEXT("respawn x"), Out.X, 700.0);
    TestEqual(TEXT("respawn y"), Out.Y, -3200.0);
    TestEqual(TEXT("respawn z"), Out.Z, 200.0);
    TestEqual(TEXT("respawn yaw"), Out.Yaw, 90.0f);
    TestTrue(TEXT("respawn vitals"), Out.bHasVitals);
    TestEqual(TEXT("respawn hp"), Out.HealthCurrent, 300);
    TestEqual(TEXT("respawn maxhp"), Out.HealthMax, 1000);
    TestEqual(TEXT("respawn mp"), Out.ManaCurrent, 150);
    TestEqual(TEXT("respawn maxmp"), Out.ManaMax, 500);
    TestEqual(TEXT("respawn echo"), Out.RequestIdEcho, FString(TEXT("sync_9")));
    // Parser->manager handoff: the manager normalizes yaw via FTeleportRule.
    TestEqual(TEXT("yaw handoff"), FTeleportRule::NormalizeYaw(Out.Yaw), 90.0f);

    FTeleportTargetData Skill;
    TestTrue(TEXT("skill shape parses"), FTeleportParser::ParseRespawnResult(ParserContract_ParseObj(
        TEXT("{\"characterId\":7,\"position\":{\"x\":1,\"y\":2,\"z\":3,\"rotationZ\":0}}")), FString(), Skill));
    TestFalse(TEXT("skill shape has no vitals"), Skill.bHasVitals);

    FTeleportTargetData Legacy;
    TestTrue(TEXT("legacy charId resolved"), FTeleportParser::ParseRespawnResult(ParserContract_ParseObj(
        TEXT("{\"position\":{\"x\":1,\"y\":2,\"z\":3,\"rotationZ\":0}}")), FString(), Legacy, 7));
    TestEqual(TEXT("legacy char"), Legacy.CharacterId, 7);

    FTeleportTargetData Bad;
    TestFalse(TEXT("null body fails"), FTeleportParser::ParseRespawnResult(TSharedPtr<FJsonObject>(), FString(), Bad));
    TestFalse(TEXT("missing charId fails"), FTeleportParser::ParseRespawnResult(
        ParserContract_ParseObj(TEXT("{\"position\":{\"x\":1,\"y\":2,\"z\":3}}")), FString(), Bad));
    TestFalse(TEXT("missing position fails"), FTeleportParser::ParseRespawnResult(
        ParserContract_ParseObj(TEXT("{\"characterId\":7}")), FString(), Bad));

    FTeleportTargetData Tp;
    TestTrue(TEXT("teleported parses"), FTeleportParser::ParsePlayerTeleported(ParserContract_ParseObj(
        TEXT("{\"characterId\":9,\"x\":585,\"y\":-3300,\"z\":200,\"rotZ\":45}")), Tp));
    TestEqual(TEXT("teleported char"), Tp.CharacterId, 9);
    TestEqual(TEXT("teleported x"), Tp.X, 585.0);
    TestFalse(TEXT("teleported never has vitals"), Tp.bHasVitals);

    FTeleportTargetData TpBad;
    TestFalse(TEXT("teleported missing coords fails"), FTeleportParser::ParsePlayerTeleported(
        ParserContract_ParseObj(TEXT("{\"characterId\":9,\"x\":1}")), TpBad));
    return true;
}
