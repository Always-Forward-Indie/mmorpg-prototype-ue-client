#include "Misc/AutomationTest.h"
#include "Utils/NetCore/VendorResultMatchRule.h"

// Vendor result matching: echo-bound application, stale/dupes dropped.
// Calls production FVendorResultMatchRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorResultMatchRuleTest, "MMO.Shop.ResultMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVendorResultMatchRuleTest::RunTest(const FString& Parameters)
{
    using R = FVendorResultMatchRule;
    // Exact echo match applies once.
    TestTrue(TEXT("match applies"), R::ShouldApply(TEXT("req-1"), TEXT("req-1"), false));
    TestFalse(TEXT("applied never re-applies"), R::ShouldApply(TEXT("req-1"), TEXT("req-1"), true));
    // Stale echo (another request's answer) is dropped.
    TestFalse(TEXT("mismatch dropped"), R::ShouldApply(TEXT("req-2"), TEXT("req-1"), false));
    // Empty echo (predates-echo server) applies once via the applied flag.
    TestTrue(TEXT("legacy echo applies once"), R::ShouldApply(TEXT("req-1"), TEXT(""), false));
    TestFalse(TEXT("legacy echo no dupe"), R::ShouldApply(TEXT("req-1"), TEXT(""), true));
    // Echo with no pending request is dropped.
    TestFalse(TEXT("no pending dropped"), R::ShouldApply(TEXT(""), TEXT("req-1"), false));
    return true;
}
