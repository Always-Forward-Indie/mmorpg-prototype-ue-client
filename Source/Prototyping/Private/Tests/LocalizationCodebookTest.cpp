#include "Misc/AutomationTest.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"

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

    // S3c wave 2: skill / emote / class / race / gender fall back the same way.
    TestEqual(TEXT("skill"), Loc->GetSkillDisplayName(TEXT("power_slash")).ToString(), FString(TEXT("Power slash")));
    TestTrue(TEXT("skill desc empty"), Loc->GetSkillDescription(TEXT("power_slash")).IsEmpty());
    TestEqual(TEXT("emote"), Loc->GetEmoteDisplayName(TEXT("dance_wild")).ToString(), FString(TEXT("Dance wild")));
    TestEqual(TEXT("class"), Loc->GetClassDisplayName(TEXT("warrior")).ToString(), FString(TEXT("Warrior")));
    TestTrue(TEXT("class desc empty"), Loc->GetClassDescription(TEXT("warrior")).IsEmpty());
    TestEqual(TEXT("race"), Loc->GetRaceDisplayName(TEXT("human")).ToString(), FString(TEXT("Human")));
    TestEqual(TEXT("gender"), Loc->GetGenderDisplayName(TEXT("male")).ToString(), FString(TEXT("Male")));

    // PickTableOrServer arbitration: table row wins, server covers misses.
    TestEqual(TEXT("pick table"),
        ULocalizationSubsystem::PickTableOrServer(FText::FromString(TEXT("Table Text")), TEXT("slug"), TEXT("Server")).ToString(),
        FString(TEXT("Table Text")));
    TestEqual(TEXT("pick server"),
        ULocalizationSubsystem::PickTableOrServer(FText::FromString(TEXT("Slug")), TEXT("slug"), TEXT("Server")).ToString(),
        FString(TEXT("Server")));
    TestEqual(TEXT("pick humanized"),
        ULocalizationSubsystem::PickTableOrServer(FText::GetEmpty(), TEXT("my_slug"), TEXT("")).ToString(),
        FString(TEXT("My slug")));
    return true;
}
