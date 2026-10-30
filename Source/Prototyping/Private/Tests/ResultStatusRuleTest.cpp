#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ResultStatusRule.h"

// Result status normalization: success packets (status only, no message)
// must discriminate as success — keying off header.message zeroes every
// total (vendor +0-gold blackout). Calls production FResultStatusRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResultStatusRuleTest, "MMO.Protocol.ResultStatus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FResultStatusRuleTest::RunTest(const FString& Parameters)
{
    using R = FResultStatusRule;
    // The vendor blackout shape: status success, no message/code.
    TestFalse(TEXT("status success is not error"), R::IsError(TEXT("success"), TEXT("")));
    TestEqual(TEXT("status success resolves success"),
        R::ResolveMessage(TEXT("success"), TEXT("")), FString(TEXT("success")));
    // Missing status + missing code (legacy shape) stays success.
    TestFalse(TEXT("empty packet is not error"), R::IsError(TEXT(""), TEXT("")));
    TestEqual(TEXT("empty packet resolves success"),
        R::ResolveMessage(TEXT(""), TEXT("")), FString(TEXT("success")));
    // Errors: code wins, status is the fallback.
    TestTrue(TEXT("status error is error"), R::IsError(TEXT("error"), TEXT("")));
    TestEqual(TEXT("bare error resolves status"),
        R::ResolveMessage(TEXT("error"), TEXT("")), FString(TEXT("error")));
    TestTrue(TEXT("code forces error"), R::IsError(TEXT("success"), TEXT("out_of_range")));
    TestEqual(TEXT("code wins over status"),
        R::ResolveMessage(TEXT("success"), TEXT("out_of_range")), FString(TEXT("out_of_range")));
    TestEqual(TEXT("error with code resolves code"),
        R::ResolveMessage(TEXT("error"), TEXT("npc_not_found")), FString(TEXT("npc_not_found")));
    // Explicit "success" code is not an error.
    TestFalse(TEXT("success code is not error"), R::IsError(TEXT("success"), TEXT("success")));
    return true;
}
