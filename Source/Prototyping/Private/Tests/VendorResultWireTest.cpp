#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/ResultStatusRule.h"
#include "Gameplay/Vendor/VendorNetworkHandler.h"

// Wire-shape regression for the vendor +0 blackout: success packets carry
// header.status and NO header.message. The full production chain
// (ResolveErrorCode -> ResolveMessage -> vendor parser) must yield parsed
// totals — keying off the raw message zeroed every total. All calls are
// production code (parser + rule + JSONParser).

static TSharedPtr<FJsonObject> StatusWire_MakeRoot(const FString& Status, bool bWithMessage)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    Header->SetStringField(TEXT("eventType"), TEXT("sellItemBatchResult"));
    Header->SetStringField(TEXT("status"), Status);
    if (bWithMessage)
        Header->SetStringField(TEXT("message"), TEXT("out_of_range"));
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetNumberField(TEXT("totalGoldReceived"), 6);
    Body->SetNumberField(TEXT("newGoldBalance"), 10);
    TArray<TSharedPtr<FJsonValue>> Items;
    TSharedPtr<FJsonObject> Line = MakeShared<FJsonObject>();
    Line->SetNumberField(TEXT("inventoryItemId"), 646);
    Line->SetNumberField(TEXT("quantity"), 1);
    Line->SetNumberField(TEXT("goldReceived"), 4);
    Items.Add(MakeShared<FJsonValueObject>(Line));
    Body->SetArrayField(TEXT("items"), Items);
    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"), Body);
    return Root;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorResultWireTest, "MMO.Shop.ResultWire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVendorResultWireTest::RunTest(const FString& Parameters)
{
    UVendorNetworkHandler* Handler = NewObject<UVendorNetworkHandler>();
    TestTrue(TEXT("handler created"), Handler != nullptr);
    if (!Handler) return false;

    // Blackout shape: success, no message anywhere.
    {
        TSharedPtr<FJsonObject> Root = StatusWire_MakeRoot(TEXT("success"), false);
        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        TestTrue(TEXT("body present"), Root->TryGetObjectField(TEXT("body"), BodyPtr));
        const FString Msg = FResultStatusRule::ResolveMessage(
            TEXT("success"), JSONParser::ResolveErrorCode(Root));
        TestEqual(TEXT("success discriminates"), Msg, FString(TEXT("success")));
        FSellItemBatchResultData Result =
            Handler->ParseSellItemBatchResult(*BodyPtr, Msg, TEXT("req-1"));
        TestTrue(TEXT("no error code"), Result.errorCode.IsEmpty());
        TestEqual(TEXT("total parsed"), Result.totalGoldReceived, 6);
        TestEqual(TEXT("balance parsed"), Result.newGoldBalance, 10);
        TestEqual(TEXT("lines parsed"), Result.items.Num(), 1);
        TestEqual(TEXT("echo bound"), Result.requestIdEcho, FString(TEXT("req-1")));
    }

    // Error shape: status error + message code.
    {
        TSharedPtr<FJsonObject> Root = StatusWire_MakeRoot(TEXT("error"), true);
        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        Root->TryGetObjectField(TEXT("body"), BodyPtr);
        const FString Msg = FResultStatusRule::ResolveMessage(
            TEXT("error"), JSONParser::ResolveErrorCode(Root));
        TestEqual(TEXT("error resolves code"), Msg, FString(TEXT("out_of_range")));
        FSellItemBatchResultData Result =
            Handler->ParseSellItemBatchResult(*BodyPtr, Msg, TEXT("req-2"));
        TestEqual(TEXT("error code set"), Result.errorCode, FString(TEXT("out_of_range")));
        TestEqual(TEXT("error totals zero"), Result.totalGoldReceived, 0);
    }
    return true;
}
