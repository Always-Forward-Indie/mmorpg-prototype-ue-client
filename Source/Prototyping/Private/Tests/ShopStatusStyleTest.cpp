#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ShopStatusStyle.h"

// Shop result-line style: success green, info neutral, error red. Single
// source of truth for the vendor / repair / skill-shop Status_Text color so
// failures never look like receipts. Calls production FShopStatusStyle.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopStatusStyleTest, "MMO.Shop.StatusStyle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FShopStatusStyleTest::RunTest(const FString& Parameters)
{
    using S = FShopStatusStyle;
    using E = EShopStatusSeverity;
    const FLinearColor Success = S::ColorFor(E::Success);
    const FLinearColor Info = S::ColorFor(E::Info);
    const FLinearColor Error = S::ColorFor(E::Error);
    // Green receipt: green channel dominates.
    TestTrue(TEXT("success is green"), Success.G > Success.R && Success.G > Success.B);
    // Red error: red channel dominates.
    TestTrue(TEXT("error is red"), Error.R > Error.G && Error.R > Error.B);
    // Info is neutral white.
    TestTrue(TEXT("info is white"), Info.Equals(FLinearColor::White));
    // All three visibly distinct.
    TestFalse(TEXT("success != error"), Success.Equals(Error));
    TestFalse(TEXT("success != info"), Success.Equals(Info));
    TestFalse(TEXT("error != info"), Error.Equals(Info));
    return true;
}
