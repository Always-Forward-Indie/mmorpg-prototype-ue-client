// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ShapeComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/DroppedItemSFX.h"

float ADroppedItemActor::FindGroundLevelAt(const FVector& Location)
{
UWorld* World = GetWorld();
if (!World) return Location.Z;

const FVector StartPos = FVector(Location.X, Location.Y,  50000.0f);
const FVector EndPos   = FVector(Location.X, Location.Y, -10000.0f);

FCollisionQueryParams QueryParams;
QueryParams.bTraceComplex = true;
QueryParams.bIgnoreTouches = true;

QueryParams.AddIgnoredActor(this);

if (APlayerController* PC = World->GetFirstPlayerController())
{
if (APawn* LocalPawn = PC->GetPawn())
{
QueryParams.AddIgnoredActor(LocalPawn);
}
}

TArray<AActor*> PawnsToIgnore;
// Registry iteration (O(registered actors)) instead of 4 world scans.
// Ground trace skips pawn hits in the loop below, so this list is only
// an optimization to reduce hit processing.
if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
{
if (UNPCManager* Npcs = GI->GetNPCManager())
{
for (ABasicNPC* Npc : Npcs->GetAllNPCs())
{
PawnsToIgnore.Add(Npc);
}
}
if (UMOBManager* Mobs = GI->GetMOBManager())
{
Mobs->ForEachMob([&](ABasicMOB* Mob) { PawnsToIgnore.Add(Mob); });
Mobs->ForEachPlayer([&](ABasicPlayer* Player) { PawnsToIgnore.Add(Player); });
}
if (UItemManager* Items = GI->GetItemManager())
{
Items->ForEachDroppedItem([&](ADroppedItemActor* Drop) { PawnsToIgnore.Add(Drop); });
}
}
QueryParams.AddIgnoredActors(PawnsToIgnore);

TArray<FHitResult> Hits;
const bool bHit = World->LineTraceMultiByChannel(Hits, StartPos, EndPos, ECC_WorldStatic, QueryParams);

if (bHit)
{
float FirstShapeHitZ = TNumericLimits<float>::Lowest();

for (const FHitResult& Hit : Hits)
{
if (!Hit.bBlockingHit) continue;

if (AActor* HitActor = Hit.GetActor())
{
if (HitActor->IsA<APawn>())
{
UE_LOG(LogDropSnap, Warning,
TEXT("[DropSnap] Trace SKIP PAWN '%s' | Pawn='%s' at Z=%.1f"),
*ItemData.item.name, *HitActor->GetName(), Hit.ImpactPoint.Z);
continue;
}
}

const UPrimitiveComponent* HitComp = Hit.GetComponent();
if (HitComp && HitComp->IsA<UShapeComponent>())
{
if (Hit.ImpactPoint.Z > FirstShapeHitZ)
FirstShapeHitZ = Hit.ImpactPoint.Z;
continue;
}

const float ResultZ = Hit.ImpactPoint.Z + 1.0f;
UE_LOG(LogDropSnap, Log,
TEXT("[DropSnap] Trace HIT '%s' | ImpactZ=%.1f -> ResultZ=%.1f | Component='%s'"),
*ItemData.item.name, Hit.ImpactPoint.Z, ResultZ,
HitComp ? *HitComp->GetName() : TEXT("null"));
return ResultZ;
}

if (FirstShapeHitZ > TNumericLimits<float>::Lowest())
{
UE_LOG(LogDropSnap, Warning,
TEXT("[DropSnap] Trace SHAPE-FALLBACK '%s' | no geometry hit, using ShapeZ=%.1f"),
*ItemData.item.name, FirstShapeHitZ);
return FirstShapeHitZ + 1.0f;
}
}

UE_LOG(LogDropSnap, Warning,
TEXT("[DropSnap] Trace MISS '%s' | Location=%s | keeping server Z=%.1f"),
*ItemData.item.name, *Location.ToString(), Location.Z);
return Location.Z;
}

void ADroppedItemActor::SnapToGround()
{
const FVector CurrentLoc = GetActorLocation();
const float GroundZ = FindGroundLevelAt(CurrentLoc);

float PivotToBottom = 0.0f;
if (ItemMesh && ItemMesh->GetStaticMesh())
{
ItemMesh->UpdateBounds();
const FBox WorldBox = ItemMesh->Bounds.GetBox();
PivotToBottom = CurrentLoc.Z - WorldBox.Min.Z;
PivotToBottom = FMath::Max(PivotToBottom, 0.0f);

UE_LOG(LogDropSnap, Log,
TEXT("[DropSnap] Bounds '%s' | BoxMin=%.1f BoxMax=%.1f BoxCenter=%.1f | PivotToBottom=%.1f"),
*ItemData.item.name,
WorldBox.Min.Z, WorldBox.Max.Z, WorldBox.GetCenter().Z,
PivotToBottom);
}

const FVector SnappedLoc = FVector(CurrentLoc.X, CurrentLoc.Y, GroundZ + PivotToBottom);
SetActorLocation(SnappedLoc, false, nullptr, ETeleportType::TeleportPhysics);

UE_LOG(LogDropSnap, Log,
TEXT("[DropSnap] Snapped '%s' | CurrentZ=%.1f GroundZ=%.1f PivotToBottom=%.1f -> FinalZ=%.1f"),
*ItemData.item.name, CurrentLoc.Z, GroundZ, PivotToBottom, SnappedLoc.Z);
}
