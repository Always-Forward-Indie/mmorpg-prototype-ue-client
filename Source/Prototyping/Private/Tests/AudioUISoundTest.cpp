#include "Misc/AutomationTest.h"
#include "Audio/AudioManager.h"
#include "Sound/SoundWaveProcedural.h"

// UI-sound router smoke (S12): every EUISoundEvent must be playable without
// a crash, with or without a mapped asset and with or without a world.
// Missing map entries are a silent no-op by design (designer assigns sounds
// in the GameInstance Blueprint); duplicate map entries are a data bug.
// Calls production UAudioManager::PlayUISound.

static int32 AudioSpec_EnumCount()
{
    const UEnum* E = StaticEnum<EUISoundEvent>();
    int32 N = 0;
    for (int32 i = 0; i < E->NumEnums(); ++i)
    {
        if (!E->HasMetaData(TEXT("Hidden"), i))
        {
            ++N;
        }
    }
    return N;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioUISoundTest, "MMO.Audio.UISounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAudioUISoundTest::RunTest(const FString& Parameters)
{
    const int32 NumEvents = AudioSpec_EnumCount();
    TestTrue(TEXT("router knows events"), NumEvents >= 18);

    // No duplicate map entries on the class default (vacuous when the
    // designer hasn't assigned sounds yet, pins future dupes).
    {
        const UAudioManager* CDO = GetDefault<UAudioManager>();
        TestTrue(TEXT("cdo available"), CDO != nullptr);
        if (CDO)
        {
            TSet<EUISoundEvent> Seen;
            bool bDup = false;
            for (const FUISoundEntry& Entry : CDO->UISounds)
            {
                if (Seen.Contains(Entry.Event)) { bDup = true; break; }
                Seen.Add(Entry.Event);
            }
            TestFalse(TEXT("no duplicate ui sound map entries"), bDup);
            // Empty map path: every event is a silent no-op, never a crash.
            // (CDO is const; the router never mutates, so const_cast is safe.)
            UAudioManager* MutableCDO = const_cast<UAudioManager*>(CDO);
            for (int32 i = 0; i < NumEvents; ++i)
            {
                MutableCDO->PlayUISound(static_cast<EUISoundEvent>(i));
            }
            TestTrue(TEXT("empty map survives all events"), true);
        }
    }

    // Present-entry + null world path: procedural sound needs no asset
    // (audio packs are gitignored and may be absent locally).
    {
        UAudioManager* Mgr = NewObject<UAudioManager>();
        TestTrue(TEXT("manager created"), Mgr != nullptr);
        if (Mgr)
        {
            USoundWaveProcedural* Proc = NewObject<USoundWaveProcedural>();
            TestTrue(TEXT("procedural sound created"), Proc != nullptr);
            FUISoundEntry Entry;
            Entry.Event = EUISoundEvent::ButtonClick;
            Entry.Sound = Proc;
            Mgr->UISounds.Add(Entry);
            for (int32 i = 0; i < NumEvents; ++i)
            {
                Mgr->PlayUISound(static_cast<EUISoundEvent>(i));
            }
            TestTrue(TEXT("mapped router survives all events"), true);
        }
    }
    return true;
}
