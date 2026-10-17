#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ClientVersion.h"
#include "MyGameInstance.h"

// ClientVersion SemVer gate: the string sent for server compat check must stay
// MAJOR.MINOR.PATCH so mismatch logic never sees garbage.
// Calls the production FClientVersion — no local re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClientVersionTest, "MMO.Protocol.ClientVersion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FClientVersionTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("current version is semver"), FClientVersion::IsSemVer(TEXT("0.1.0")));
    TestFalse(TEXT("reject short"), FClientVersion::IsSemVer(TEXT("0.1")));
    TestFalse(TEXT("reject prefix"), FClientVersion::IsSemVer(TEXT("v1.0.0")));
    TestFalse(TEXT("reject empty"), FClientVersion::IsSemVer(TEXT("")));
    TestFalse(TEXT("reject non-numeric"), FClientVersion::IsSemVer(TEXT("0.1.x")));
    // The shipped default (sent as body.clientVersion on login/register) must itself be SemVer.
    // Bump deliberately: the login gate compares this against [min_client_version, max_client_version].
    TestTrue(TEXT("shipped default is semver"), FClientVersion::IsSemVer(GetDefault<UMyGameInstance>()->ClientVersion));

    // Numeric (not lexicographic) ordering: 0.1.10 > 0.1.2.
    bool bValid = false;
    TestEqual(TEXT("patch ordering"), FClientVersion::Compare(TEXT("0.1.2"), TEXT("0.1.10"), bValid), -1);
    TestTrue(TEXT("ordering valid"), bValid);
    TestEqual(TEXT("equal versions"), FClientVersion::Compare(TEXT("0.1.2"), TEXT("0.1.2"), bValid), 0);
    TestEqual(TEXT("major wins"), FClientVersion::Compare(TEXT("1.0.0"), TEXT("0.9.9"), bValid), 1);
    FClientVersion::Compare(TEXT("garbage"), TEXT("0.1.2"), bValid);
    TestFalse(TEXT("garbage invalid"), bValid);

    // Compat gate mirrors the server [min, max] check (inclusive).
    TestTrue(TEXT("inside range"), FClientVersion::IsCompatible(TEXT("0.1.2"), TEXT("0.1.2"), TEXT("0.1.4")));
    TestTrue(TEXT("at max boundary"), FClientVersion::IsCompatible(TEXT("0.1.4"), TEXT("0.1.2"), TEXT("0.1.4")));
    TestFalse(TEXT("below min"), FClientVersion::IsCompatible(TEXT("0.1.1"), TEXT("0.1.2"), TEXT("0.1.4")));
    TestFalse(TEXT("above max"), FClientVersion::IsCompatible(TEXT("0.1.5"), TEXT("0.1.2"), TEXT("0.1.4")));
    TestFalse(TEXT("garbage never gates"), FClientVersion::IsCompatible(TEXT("?.?.?"), TEXT("0.1.2"), TEXT("0.1.4")));
    return true;
}
