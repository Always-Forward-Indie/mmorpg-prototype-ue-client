#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/ItemStruct.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystemComponent.h"

UEquipmentVisualComponent::UEquipmentVisualComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEquipmentVisualComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UEquipmentVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unbind delegate before we are destroyed to avoid dangling callbacks
    if (EquipmentManager)
    {
        EquipmentManager->OnEquipmentStateChangedDelegate.RemoveDynamic(
            this, &UEquipmentVisualComponent::RefreshAllSlots);
    }

    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------

void UEquipmentVisualComponent::Initialize(UEquipmentManager* InEquipmentManager,
                                            UItemManager*      InItemManager)
{
    if (!InEquipmentManager || !InItemManager)
    {
        UE_LOG(LogTemp, Error,
               TEXT("EquipmentVisualComponent: Initialize called with null parameters"));
        return;
    }

    EquipmentManager = InEquipmentManager;
    ItemManager      = InItemManager;

    // Bind to state-change delegate so we rebuild visuals whenever equipment
    // changes (equip, unequip, or full state refresh from server)
    EquipmentManager->OnEquipmentStateChangedDelegate.AddDynamic(
        this, &UEquipmentVisualComponent::RefreshAllSlots);

    // Immediately apply whatever the manager already holds (e.g. we initialised
    // after the first EQUIPMENT_STATE packet was already processed)
    RefreshAllSlots(EquipmentManager->GetEquipmentState());

    UE_LOG(LogTemp, Log, TEXT("EquipmentVisualComponent: Initialized"));
}

void UEquipmentVisualComponent::InitializeForRemotePlayer(UItemManager* InItemManager)
{
    if (!InItemManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipmentVisualComponent: InitializeForRemotePlayer called with null ItemManager"));
        return;
    }
    ItemManager = InItemManager;
    UE_LOG(LogTemp, Log, TEXT("EquipmentVisualComponent: Initialized for remote player (CharID=%d)"), OwnerCharacterId);
}

void UEquipmentVisualComponent::HandleRemoteEquipmentState(const FEquipmentStateData& State)
{
    // Filter: only process state for our specific remote character
    if (OwnerCharacterId <= 0 || State.characterId != OwnerCharacterId)
    {
        UE_LOG(LogTemp, Log,
            TEXT("EquipmentVisualComponent: HandleRemoteEquipmentState skipped — OwnerCharID=%d vs State.charID=%d"),
            OwnerCharacterId, State.characterId);
        return;
    }
    UE_LOG(LogTemp, Warning,
        TEXT("EquipmentVisualComponent: HandleRemoteEquipmentState applying for CharID=%d (%d slot(s)) owner=%s"),
        State.characterId, State.slots.Num(),
        GetOwner() ? *GetOwner()->GetName() : TEXT("null"));
    RefreshAllSlots(State);
}

// ---------------------------------------------------------------------------

void UEquipmentVisualComponent::RefreshAllSlots(const FEquipmentStateData& State)
{
    UE_LOG(LogTemp, Log,
           TEXT("EquipmentVisualComponent: RefreshAllSlots called � %d slot(s) in state, owner=%s"),
           State.slots.Num(), GetOwner() ? *GetOwner()->GetName() : TEXT("null"));

    // 1. Collect slugs that are currently occupied on the server
    TSet<FString> OccupiedSlugs;
    for (const auto& Pair : State.slots)
    {
        if (Pair.Value.bIsOccupied)
        {
            OccupiedSlugs.Add(Pair.Key);
        }
    }

    // 2. Remove meshes for slots that are no longer occupied
    TArray<FString> CurrentSlugs;
    SlotComponents.GetKeys(CurrentSlugs);
    for (const FString& Slug : CurrentSlugs)
    {
        if (!OccupiedSlugs.Contains(Slug))
        {
            DestroySlotComponent(Slug);
        }
    }

    // 3. Create / update meshes for occupied slots
    for (const auto& Pair : State.slots)
    {
        if (Pair.Value.bIsOccupied && !Pair.Value.itemSlug.IsEmpty())
        {
            AttachEquippedMesh(Pair.Key, Pair.Value.itemSlug);
        }
    }
}

void UEquipmentVisualComponent::ClearSlotMesh(const FString& SlotSlug)
{
    DestroySlotComponent(SlotSlug);
}

void UEquipmentVisualComponent::ClearAllMeshes()
{
    TArray<FString> Keys;
    SlotComponents.GetKeys(Keys);
    for (const FString& Key : Keys)
    {
        DestroySlotComponent(Key);
    }

    TArray<FString> VFXKeys;
    SlotVFXComponents.GetKeys(VFXKeys);
    for (const FString& Key : VFXKeys)
    {
        DestroySlotVFX(Key);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

USceneComponent* UEquipmentVisualComponent::AttachEquippedMesh(const FString& SlotSlug,
                                                                const FString& ItemSlug)
{
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("EquipmentVisualComponent: AttachEquippedMesh — ItemManager is null for slot '%s' item '%s'"),
            *SlotSlug, *ItemSlug);
        return nullptr;
    }
    if (!GetOwner())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("EquipmentVisualComponent: AttachEquippedMesh — GetOwner() is null for slot '%s' item '%s'"),
            *SlotSlug, *ItemSlug);
        return nullptr;
    }

    FItemVisualData VisualData = ItemManager->GetItemVisualDataBySlug(ItemSlug);

    // If no socket is defined, this item has no in-world visual when equipped
    if (VisualData.EquipSocketName == NAME_None)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("EquipmentVisualComponent: '%s' has no EquipSocketName in ItemVisualsDataTable � skipping visual for slot '%s'. Check that ItemVisualsDataTable is assigned in GameInstance BP and the row for this slug exists."),
               *ItemSlug, *SlotSlug);
        // Still remove old component in case item was swapped with one that has no visual
        DestroySlotComponent(SlotSlug);
        return nullptr;
    }

    // Resolve the character mesh to attach to (must be a SkeletalMeshComponent)
    USkeletalMeshComponent* CharMesh =
        GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (!CharMesh)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("EquipmentVisualComponent: Owner has no USkeletalMeshComponent"));
        return nullptr;
    }

    // -----------------------------------------------------------------------
    // Decide which mesh type to use.
    // Priority: EquippedStaticMesh > EquippedSkeletalMesh
    // -----------------------------------------------------------------------

    const bool bHasStaticMesh   = !VisualData.EquippedStaticMesh.IsNull();
    const bool bHasSkelMesh     = !VisualData.EquippedSkeletalMesh.IsNull();

    if (!bHasStaticMesh && !bHasSkelMesh)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("EquipmentVisualComponent: '%s' found in DataTable but both EquippedStaticMesh and EquippedSkeletalMesh are null � assign at least one mesh in the DataTable row for slot '%s' to see the item on the character."),
               *ItemSlug, *SlotSlug);
        DestroySlotComponent(SlotSlug);
        return nullptr;
    }

    // Destroy the previous component for this slot so we start clean
    DestroySlotComponent(SlotSlug);

    USceneComponent* NewComp = nullptr;

    if (bHasStaticMesh)
    {
        UStaticMesh* Mesh = VisualData.EquippedStaticMesh.LoadSynchronous();
        if (!Mesh)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("EquipmentVisualComponent: Failed to load EquippedStaticMesh for '%s'"),
                   *ItemSlug);
            return nullptr;
        }

        UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(GetOwner(),
            *FString::Printf(TEXT("EquipMesh_%s"), *SlotSlug));
        SMC->SetStaticMesh(Mesh);
        SMC->RegisterComponent();
        SMC->AttachToComponent(CharMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            VisualData.EquipSocketName);
        SMC->SetRelativeTransform(VisualData.EquippedRelativeTransform);
        SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NewComp = SMC;
    }
    else // bHasSkelMesh
    {
        USkeletalMesh* Mesh = VisualData.EquippedSkeletalMesh.LoadSynchronous();
        if (!Mesh)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("EquipmentVisualComponent: Failed to load EquippedSkeletalMesh for '%s'"),
                   *ItemSlug);
            return nullptr;
        }

        USkeletalMeshComponent* SkMC = NewObject<USkeletalMeshComponent>(GetOwner(),
            *FString::Printf(TEXT("EquipMesh_%s"), *SlotSlug));
        SkMC->SetSkeletalMesh(Mesh);
        SkMC->RegisterComponent();
        SkMC->AttachToComponent(CharMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            VisualData.EquipSocketName);
        SkMC->SetRelativeTransform(VisualData.EquippedRelativeTransform);
        SkMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NewComp = SkMC;
    }

    if (NewComp)
    {
        SlotComponents.Add(SlotSlug, NewComp);
        SlotItemSlugs.Add(SlotSlug, ItemSlug);
        UE_LOG(LogTemp, Log,
               TEXT("EquipmentVisualComponent: Attached '%s' to socket '%s' for slot '%s'"),
               *ItemSlug, *VisualData.EquipSocketName.ToString(), *SlotSlug);

        // Spawn persistent idle VFX (enchantment glow, rarity aura, etc.)
        SpawnEquippedIdleVFX(SlotSlug, VisualData, CharMesh);
    }

    return NewComp;
}

void UEquipmentVisualComponent::DestroySlotComponent(const FString& SlotSlug)
{
    // Also clean up any associated VFX
    DestroySlotVFX(SlotSlug);

    if (USceneComponent** Found = SlotComponents.Find(SlotSlug))
    {
        if (*Found && (*Found)->IsValidLowLevel())
        {
            (*Found)->DestroyComponent();
        }
        SlotComponents.Remove(SlotSlug);
    }

    SlotItemSlugs.Remove(SlotSlug);
}

void UEquipmentVisualComponent::DestroySlotVFX(const FString& SlotSlug)
{
    if (UNiagaraComponent** Found = SlotVFXComponents.Find(SlotSlug))
    {
        if (*Found && (*Found)->IsValidLowLevel())
        {
            (*Found)->Deactivate();
            (*Found)->DestroyComponent();
        }
        SlotVFXComponents.Remove(SlotSlug);
    }
}

USceneComponent* UEquipmentVisualComponent::GetSlotComponent(const FString& SlotSlug) const
{
    if (USceneComponent* const* Found = SlotComponents.Find(SlotSlug))
    {
        return *Found;
    }
    return nullptr;
}

FString UEquipmentVisualComponent::GetItemSlugForSlot(const FString& SlotSlug) const
{
    if (const FString* Found = SlotItemSlugs.Find(SlotSlug))
    {
        return *Found;
    }
    return FString();
}

void UEquipmentVisualComponent::SpawnEquippedIdleVFX(const FString& SlotSlug,
                                                      const FItemVisualData& VisualData,
                                                      USkeletalMeshComponent* CharMesh)
{
    // Destroy previous VFX for this slot
    DestroySlotVFX(SlotSlug);

    if (!CharMesh || !GetOwner()) return;

    // Prefer Niagara, fall back to Cascade
    if (!VisualData.EquippedIdleVFX.IsNull())
    {
        UNiagaraSystem* NiagaraSys = VisualData.EquippedIdleVFX.LoadSynchronous();
        if (NiagaraSys)
        {
            UNiagaraComponent* NC = NewObject<UNiagaraComponent>(GetOwner(),
                *FString::Printf(TEXT("EquipVFX_%s"), *SlotSlug));
            NC->SetAsset(NiagaraSys);
            NC->RegisterComponent();
            NC->AttachToComponent(CharMesh,
                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                VisualData.EquipSocketName);
            NC->SetRelativeTransform(VisualData.EquippedRelativeTransform);
            NC->Activate(true);

            SlotVFXComponents.Add(SlotSlug, NC);

            UE_LOG(LogTemp, Log,
                   TEXT("EquipmentVisualComponent: Spawned idle Niagara VFX for slot '%s' on socket '%s'"),
                   *SlotSlug, *VisualData.EquipSocketName.ToString());
        }
    }
}
