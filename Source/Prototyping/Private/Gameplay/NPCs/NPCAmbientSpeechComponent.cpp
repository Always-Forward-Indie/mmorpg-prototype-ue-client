#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "Gameplay/UI/NameplateManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

UNPCAmbientSpeechComponent::UNPCAmbientSpeechComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;  // enabled only when proximity lines exist
}

void UNPCAmbientSpeechComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UNPCAmbientSpeechComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopAmbientSpeech();
    Super::EndPlay(EndPlayReason);
}

void UNPCAmbientSpeechComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bHasData || !bHasProximityLines) return;

    // Proximity check against local player pawn
    ACharacter* LocalPawn = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (!LocalPawn || !GetOwner()) return;

    const FVector OwnerLoc = GetOwner()->GetActorLocation();
    const FVector PlayerLoc = LocalPawn->GetActorLocation();

    for (const FAmbientSpeechPoolData& Pool : AmbientData.pools)
    {
        for (const FAmbientSpeechLineData& Line : Pool.lines)
        {
            if (Line.triggerType != TEXT("proximity")) continue;
            if (TriggeredProximityLines.Contains(Line.id)) continue;

            const float Dist = FVector::Dist(OwnerLoc, PlayerLoc);
            if (Dist <= Line.triggerRadius)
            {
                TriggeredProximityLines.Add(Line.id);
                ShowSpeechLine(Line);
            }
        }
    }
}

// ---------------------------------------------------------------------------

void UNPCAmbientSpeechComponent::SetAmbientData(const FAmbientSpeechNPCData& Data)
{
    StopAmbientSpeech();

    AmbientData = Data;
    bHasData = true;
    bHasProximityLines = false;

    int32 TotalLines = 0;
    for (const FAmbientSpeechPoolData& P : Data.pools) TotalLines += P.lines.Num();
    UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] NPC %d: SetAmbientData — %d pools, %d total lines, interval [%d–%d]s"),
        Data.npcId, Data.pools.Num(), TotalLines, Data.minIntervalSec, Data.maxIntervalSec);

    if (Data.pools.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AmbientSpeech] NPC %d: pools array is EMPTY — nothing will play"), Data.npcId);
        return;
    }

    // Check whether any proximity lines exist (enables Tick)
    for (const FAmbientSpeechPoolData& Pool : Data.pools)
    {
        for (const FAmbientSpeechLineData& Line : Pool.lines)
        {
            if (Line.triggerType == TEXT("proximity"))
            {
                bHasProximityLines = true;
                break;
            }
        }
        if (bHasProximityLines) break;
    }

    SetComponentTickEnabled(bHasProximityLines);
    UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] NPC %d: proximity lines=%s — scheduling periodic timer"),
        AmbientData.npcId, bHasProximityLines ? TEXT("YES") : TEXT("no"));
    ScheduleNextPeriodicTimer();
}

void UNPCAmbientSpeechComponent::StopAmbientSpeech()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PeriodicTimer);
    }
    LineCooldowns.Empty();
    TriggeredProximityLines.Empty();
    bHasData = false;
    bHasProximityLines = false;
    SetComponentTickEnabled(false);
}

// ---------------------------------------------------------------------------
// Periodic timer
// ---------------------------------------------------------------------------

void UNPCAmbientSpeechComponent::ScheduleNextPeriodicTimer()
{
    if (!bHasData) return;
    UWorld* World = GetWorld();
    if (!World) return;

    const float Interval = FMath::FRandRange(
        FMath::Max(AmbientData.minIntervalSec, 1.f),
        FMath::Max(AmbientData.maxIntervalSec, AmbientData.minIntervalSec + 1.f));

    UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] NPC %d: next periodic fire in %.1fs"), AmbientData.npcId, Interval);

    World->GetTimerManager().SetTimer(PeriodicTimer, this,
        &UNPCAmbientSpeechComponent::OnPeriodicTimerFired, Interval, false);
}

void UNPCAmbientSpeechComponent::OnPeriodicTimerFired()
{
    UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] NPC %d: periodic timer fired"), AmbientData.npcId);
    FAmbientSpeechLineData Line;
    if (PickPeriodicLine(Line))
    {
        UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] NPC %d: picked line id=%d key='%s'"),
            AmbientData.npcId, Line.id, *Line.lineKey);
        ShowSpeechLine(Line);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[AmbientSpeech] NPC %d: PickPeriodicLine returned nothing (all on cooldown?)"), AmbientData.npcId);
    }
    ScheduleNextPeriodicTimer();
}

// ---------------------------------------------------------------------------
// Line picking
// ---------------------------------------------------------------------------

bool UNPCAmbientSpeechComponent::PickPeriodicLine(FAmbientSpeechLineData& OutLine) const
{
    // Pools are expected to arrive sorted by priority descending from the server.
    // Walk them in order; try the first pool that has at least one eligible line.
    for (const FAmbientSpeechPoolData& Pool : AmbientData.pools)
    {
        // Collect eligible lines from this pool (periodic only, cooldown passed)
        TArray<const FAmbientSpeechLineData*> Eligible;
        int32 TotalWeight = 0;

        for (const FAmbientSpeechLineData& Line : Pool.lines)
        {
            if (Line.triggerType != TEXT("periodic")) continue;

            // Check cooldown
            const float* LastShown = LineCooldowns.Find(Line.id);
            if (LastShown && (Now() - *LastShown) < Line.cooldownSec) continue;

            Eligible.Add(&Line);
            TotalWeight += FMath::Max(Line.weight, 1);
        }

        if (Eligible.IsEmpty())
        {
            UE_LOG(LogTemp, Verbose, TEXT("[AmbientSpeech] NPC %d: pool priority=%d has no eligible periodic lines (all on cooldown)"),
                AmbientData.npcId, Pool.priority);
            continue;  // no eligible lines in this pool, try lower priority
        }

        // Weighted random selection
        int32 Roll = FMath::RandRange(0, TotalWeight - 1);
        int32 Accumulated = 0;
        for (const FAmbientSpeechLineData* L : Eligible)
        {
            Accumulated += FMath::Max(L->weight, 1);
            if (Roll < Accumulated)
            {
                OutLine = *L;
                return true;
            }
        }
        // Fallback: pick last (should not normally reach here)
        if (!Eligible.IsEmpty())
        {
            OutLine = *Eligible.Last();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void UNPCAmbientSpeechComponent::ShowSpeechLine(const FAmbientSpeechLineData& Line)
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (!GI) { UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] ShowSpeechLine: no GameInstance")); return; }

    ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>();
    if (!LocSys) { UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] ShowSpeechLine: no LocalizationSubsystem")); return; }

    // Resolve text and display duration
    FAmbientSpeechLineDefinition Def;
    FText SpeechText;
    float Duration = DefaultDisplayDuration;

    if (LocSys->GetNPCSpeechLineDefinition(Line.lineKey, Def))
    {
        SpeechText = Def.speechText;
        if (Def.DisplayDuration > 0.f) Duration = Def.DisplayDuration;
        UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] key='%s' resolved via DataTable: '%s'"),
            *Line.lineKey, *SpeechText.ToString());
    }
    else
    {
        SpeechText = LocSys->GetNPCSpeechText(Line.lineKey);
        UE_LOG(LogTemp, Warning, TEXT("[AmbientSpeech] key='%s' NOT in DataTable — fallback: '%s'"),
            *Line.lineKey, *SpeechText.ToString());
    }

    if (SpeechText.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] key='%s' resolved to EMPTY text — bubble suppressed"), *Line.lineKey); return; }

    // Record cooldown
    LineCooldowns.Add(Line.id, Now());

    // Find NameplateManager via UIManager on the local player pawn
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
        {
            if (UUIManager* UIMgr = Player->GetUIManager())
            {
                if (UNameplateManager* NMgr = UIMgr->GetNameplateManager())
                {
                    UE_LOG(LogTemp, Log, TEXT("[AmbientSpeech] NPC %d: calling ShowNPCSpeechBubble '%s' %.1fs"),
                        AmbientData.npcId, *SpeechText.ToString(), Duration);
                    NMgr->ShowNPCSpeechBubble(GetOwner(), SpeechText, Duration);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] NPC %d: GetNameplateManager() returned null"), AmbientData.npcId);
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] NPC %d: GetUIManager() returned null"), AmbientData.npcId);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] NPC %d: PlayerController pawn is not ABasicPlayer (pawn=%s)"),
                AmbientData.npcId, PC->GetPawn() ? *PC->GetPawn()->GetClass()->GetName() : TEXT("null"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[AmbientSpeech] NPC %d: PlayerController is NULL"), AmbientData.npcId);
    }

    // Optional: play sound
    if (!Def.SpeechSound.IsNull())
    {
        USoundBase* Sound = Def.SpeechSound.LoadSynchronous();
        if (Sound && GetOwner())
        {
            UGameplayStatics::SpawnSoundAttached(Sound, GetOwner()->GetRootComponent(), NAME_None,
                GetOwner()->GetActorLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
                true, 1.0f, 1.0f, 0.0f, nullptr, nullptr, true);
        }
    }
}

float UNPCAmbientSpeechComponent::Now() const
{
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.f;
}
