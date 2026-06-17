// Entity Audio Data — shared audio profile and per-skill voice overrides
// Used by all entity types (Player, MOB, NPC) via AudioProfileId FK.
//
// Row key examples:
//   Players: "warrior_m", "warrior_f", "mage_m", "archer_f"
//   Mobs:    "wolf", "goblin_grunt", "goblin_shaman", "skeleton_warrior"
//   NPCs:    "blacksmith", "innkeeper", "guard_captain"
//   Shared:  "giant_humanoid" — multiple mob types, one sound set

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EntityAudioData.generated.h"

// ============================================================
// Entity Skill Voice Override — per-entity per-skill voice pool
// Row key format: "{audioProfileId}|{skillSlug}" (e.g. "warrior_m|fireball")
//
// Priority chain for cast-start voice:
//   P1: FSkillDefinitionData.castStartVoice  — same sound for ALL casters of this skill
//   P2: DT_EntitySkillVoiceOverrides["warrior_m|fireball"].CastStartVoice  — per-entity per-skill pool
//   P3: FEntityAudioProfile.VoiceCastStart[]  — per-entity generic fallback pool
// ============================================================
USTRUCT(BlueprintType)
struct PROTOTYPING_API FEntitySkillVoiceOverride : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Override")
    TArray<TSoftObjectPtr<USoundBase>> CastStartVoice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Override")
    TArray<TSoftObjectPtr<USoundBase>> CastReleaseVoice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Override")
    TArray<TSoftObjectPtr<USoundBase>> VoiceAttack;
};

// ============================================================
// Entity Audio Profile — shared audio definition for one entity
// ============================================================
USTRUCT(BlueprintType)
struct FEntityAudioProfile : public FTableRowBase
{
    GENERATED_BODY()

    // ---- Voice ------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
    TArray<TSoftObjectPtr<USoundBase>> VoiceAttack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
    TArray<TSoftObjectPtr<USoundBase>> VoiceCastStart;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
    TArray<TSoftObjectPtr<USoundBase>> VoiceCastRelease;

    // ---- Combat -----------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> SwingSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> HitReceived;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> Death;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> Revive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> Aggro;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> AttackGeneric;

    // ---- Heal / Progression -----------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
    TSoftObjectPtr<USoundBase> HealReceived;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
    TSoftObjectPtr<USoundBase> LevelUp;

    // ---- Movement / Ambient ----------------------------------
    /** Footwear type for composite surface lookup: "boot", "hoof", "paw", "barefoot", "claw".
     *  Combined with PhysMat name: "PM_Stone_hoof" → "PM_Stone" (fallback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FName FootwearType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    TArray<TSoftObjectPtr<USoundBase>> Footsteps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
    TArray<TSoftObjectPtr<USoundBase>> IdleAmbient;

    // ---- NPC Social -------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Social")
    TSoftObjectPtr<USoundBase> GreetingSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Social")
    TSoftObjectPtr<USoundBase> InteractSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Social")
    TSoftObjectPtr<USoundBase> FarewellSound;

    // ---- Attenuation ------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attenuation")
    TSoftObjectPtr<USoundAttenuation> DefaultAttenuation;
};
