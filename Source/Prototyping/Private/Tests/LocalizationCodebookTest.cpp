#include "Misc/AutomationTest.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"

// S3a codebook contract: attribute / item-type / rarity / equip-slot /
// item-set getters resolve display names by slug. With no tables assigned
// (content lands in S3b) every getter falls back to a humanised slug —
// byte-identical to the pre-S3a inline hacks in the tooltips.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodebookFallbackContractTest, "MMO.Protocol.CodebookFallbackContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCodebookFallbackContractTest::RunTest(const FString& Parameters)
{
    // Fallback math itself (shared helper behind every getter).
    TestEqual(TEXT("humanize"), JSONParser::HumanizeSlugForDisplay(TEXT("forest_wolf")), FString(TEXT("Forest wolf")));

    // Real getters via the class default object: no tables assigned (content
    // lands in S3b), so every getter must take the humanised-slug fallback.
    // (Subsystems can't NewObject standalone; the CDO exercises the same code.)
    const ULocalizationSubsystem* Loc = GetDefault<ULocalizationSubsystem>();
    TestTrue(TEXT("cdo available"), Loc != nullptr);
    if (!Loc) return false;

    TestEqual(TEXT("attr"), Loc->GetAttributeDisplayName(TEXT("max_health")).ToString(), FString(TEXT("Max health")));
    TestEqual(TEXT("type"), Loc->GetItemTypeDisplayName(TEXT("weapon")).ToString(), FString(TEXT("Weapon")));
    TestEqual(TEXT("rarity"), Loc->GetRarityDisplayName(TEXT("uncommon")).ToString(), FString(TEXT("Uncommon")));
    TestEqual(TEXT("slot"), Loc->GetEquipSlotDisplayName(TEXT("main_hand")).ToString(), FString(TEXT("Main hand")));
    TestEqual(TEXT("set"), Loc->GetItemSetDisplayName(TEXT("wolf_pack")).ToString(), FString(TEXT("Wolf pack")));
    TestTrue(TEXT("empty in empty out"), Loc->GetAttributeDisplayName(TEXT("")).IsEmpty());
    TestTrue(TEXT("empty type"), Loc->GetItemTypeDisplayName(TEXT("")).IsEmpty());
    return true;
}
