#include "Misc/AutomationTest.h"
#include "Utils/NetCore/AuthValidation.h"

// Client-side credential length rules (login 3..20, password 8..100).
// Calls the production FAuthValidation used by UAuthenticationManager —
// no local re-implementation. Bounds mirror the server contract.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthValidationTest, "MMO.Auth.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
FString MakeString(int32 Len)
{
    FString S;
    for (int32 i = 0; i < Len; ++i) { S.AppendChar(TEXT('a')); }
    return S;
}
}

bool FAuthValidationTest::RunTest(const FString& Parameters)
{
    using V = FAuthValidation;
    TestEqual(TEXT("login empty"), V::ValidateLogin(TEXT("")), EAuthValueError::Empty);
    TestEqual(TEXT("login too short"), V::ValidateLogin(TEXT("ab")), EAuthValueError::TooShort);
    TestEqual(TEXT("login min ok"), V::ValidateLogin(MakeString(3)), EAuthValueError::Ok);
    TestEqual(TEXT("login max ok"), V::ValidateLogin(MakeString(20)), EAuthValueError::Ok);
    TestEqual(TEXT("login too long"), V::ValidateLogin(MakeString(21)), EAuthValueError::TooLong);
    TestEqual(TEXT("password empty"), V::ValidatePassword(TEXT("")), EAuthValueError::Empty);
    TestEqual(TEXT("password too short"), V::ValidatePassword(MakeString(7)), EAuthValueError::TooShort);
    TestEqual(TEXT("password min ok"), V::ValidatePassword(MakeString(8)), EAuthValueError::Ok);
    TestEqual(TEXT("password max ok"), V::ValidatePassword(MakeString(100)), EAuthValueError::Ok);
    TestEqual(TEXT("password too long"), V::ValidatePassword(MakeString(101)), EAuthValueError::TooLong);
    TestTrue(TEXT("IsOk true"), V::IsOk(EAuthValueError::Ok));
    TestFalse(TEXT("IsOk false"), V::IsOk(EAuthValueError::TooShort));
    return true;
}
