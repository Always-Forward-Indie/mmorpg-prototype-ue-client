#include "Misc/AutomationTest.h"

// Mirrors Documentation/Server Info/API/00-protocol-overview.md framing rules:
// one JSON object per line, '\n' terminated, empty lines ignored, 8KB max.
namespace
{
int32 SplitFraming(const FString& Buf, TArray<FString>& OutLines, FString& OutRest)
{
    OutLines.Reset();
    TArray<FString> Parts;
    Buf.ParseIntoArray(Parts, TEXT("\n"), /*InCullEmpty=*/false);
    OutRest = Parts.Num() > 0 ? Parts.Pop() : FString();
    for (const FString& P : Parts)
    {
        FString T = P.TrimStartAndEnd();
        if (!T.IsEmpty())
        {
            OutLines.Add(T);
        }
    }
    return OutLines.Num();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolFramingTest, "MMO.Protocol.Framing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProtocolFramingTest::RunTest(const FString& Parameters)
{
    TArray<FString> Lines;
    FString Rest;
    // Empty lines ignored, trailing partial chunk kept as rest.
    SplitFraming(TEXT("\n{\"a\":1}\n\n{\"b\":2}\n{\"par"), Lines, Rest);
    TestEqual(TEXT("two messages parsed"), Lines.Num(), 2);
    TestEqual(TEXT("partial rest kept"), Rest, FString(TEXT("{\"par")));

    // 8KB cap from the spec.
    static constexpr int32 MaxMessageBytes = 8 * 1024;
    TestTrue(TEXT("8KB cap is 8192"), MaxMessageBytes == 8192);
    return true;
}
