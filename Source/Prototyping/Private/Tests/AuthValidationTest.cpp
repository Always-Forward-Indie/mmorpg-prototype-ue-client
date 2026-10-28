#include "Misc/AutomationTest.h"
#include "Utils/NetCore/AuthValidation.h"
#include "Authentication/LoginFlowTypes.h"

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

// Login-flow email mask + char-name rules (LoginFlowValidation, UX-level).
// Email is required and must match local@domain.tld — bare "@" is rejected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthLoginFlowEmailTest, "MMO.Auth.LoginFlowEmail",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAuthLoginFlowEmailTest::RunTest(const FString& Parameters)
{
    namespace V = LoginFlowValidation;
    FText Err;
    TestTrue(TEXT("plain valid"), V::IsEmailValid(TEXT("hero@example.com"), Err));
    TestTrue(TEXT("subdomain valid"), V::IsEmailValid(TEXT("a.b+tag@mail.example.co"), Err));
    TestFalse(TEXT("empty rejected"), V::IsEmailValid(TEXT(""), Err));
    TestFalse(TEXT("no at rejected"), V::IsEmailValid(TEXT("notanemail"), Err));
    TestFalse(TEXT("bare at rejected"), V::IsEmailValid(TEXT("sdfsfdfsd@fff"), Err));
    TestFalse(TEXT("no tld rejected"), V::IsEmailValid(TEXT("a@b"), Err));
    TestFalse(TEXT("leading at rejected"), V::IsEmailValid(TEXT("@example.com"), Err));
    TestFalse(TEXT("spaces rejected"), V::IsEmailValid(TEXT("a b@c.com"), Err));
    // Caps stay in sync with the input truncation in NativeConstruct.
    TestTrue(TEXT("login cap"), V::MaxLoginLen == 20);
    TestTrue(TEXT("password cap"), V::MaxPasswordLen == 100);
    TestTrue(TEXT("charname cap"), V::MaxCharNameLen == 20);
    return true;
}

// Gender row match: the SELECTED display must match the row — a self-compare
// (RowDisplay vs the row's own Label/Name) matches the first row forever and
// silently forces male. Pinned after the create-preview/select-screen bug.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoginFlowGenderMatchTest, "MMO.Auth.LoginFlowGender",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FLoginFlowGenderMatchTest::RunTest(const FString& Parameters)
{
    namespace V = LoginFlowValidation;
    // Selected "Female" must match the female row only.
    TestTrue(TEXT("female matches female row"),
        V::GenderOptionMatches(TEXT("Female"), TEXT("Female"), TEXT("Female"), TEXT("female")));
    TestFalse(TEXT("female does not match male row"),
        V::GenderOptionMatches(TEXT("Female"), TEXT("Male"), TEXT("Male"), TEXT("male")));
    // Symmetrically, "Male" must not match the female row.
    TestTrue(TEXT("male matches male row"),
        V::GenderOptionMatches(TEXT("Male"), TEXT("Male"), TEXT("Male"), TEXT("male")));
    TestFalse(TEXT("male does not match female row"),
        V::GenderOptionMatches(TEXT("Male"), TEXT("Female"), TEXT("Female"), TEXT("female")));
    // Slug/name identity also resolve (server-shaped payloads).
    TestTrue(TEXT("slug identity"), V::GenderOptionMatches(TEXT("female"), TEXT("Female"), TEXT("Female"), TEXT("female")));
    return true;
}
