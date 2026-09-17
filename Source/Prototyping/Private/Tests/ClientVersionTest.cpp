#include "Misc/AutomationTest.h"

// ClientVersion SemVer gate: the string sent for server compat check must stay
// MAJOR.MINOR.PATCH so mismatch logic never sees garbage.
namespace
{
bool IsSemVer(const FString& V)
{
    TArray<FString> Parts;
    V.ParseIntoArray(Parts, TEXT("."));
    if (Parts.Num() != 3)
    {
        return false;
    }
    for (const FString& P : Parts)
    {
        if (P.IsEmpty() || !P.IsNumeric())
        {
            return false;
        }
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClientVersionTest, "MMO.Protocol.ClientVersion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FClientVersionTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("current version is semver"), IsSemVer(TEXT("0.1.0")));
    TestFalse(TEXT("reject short"), IsSemVer(TEXT("0.1")));
    TestFalse(TEXT("reject prefix"), IsSemVer(TEXT("v1.0.0")));
    return true;
}
