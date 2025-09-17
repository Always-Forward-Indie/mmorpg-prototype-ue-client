# NPC System Setup Guide

## Overview
The NPC system automatically loads NPCs from server data and applies visual/audio assets based on their `slug` values using a DataTable system.

## Setting Up NPCDefinitionTable

### 1. Create NPCDefinitionTable in Unreal Editor

1. **Create DataTable**:
   - Right-click in Content Browser
   - Go to `Miscellaneous` ? `Data Table`
   - Choose `NPCDefinition` as the Row Structure
   - Name it `DT_NPCDefinitions` (or similar)

2. **Configure MyGameInstance**:
   - Open your GameInstance Blueprint (or set in C++)
   - Set `NPCDefinitionTable` to your created DataTable

### 2. DataTable Structure

Each row in the DataTable corresponds to an NPC `slug` and contains:

```cpp
FNPCDefinition
??? NPCType (FName) - The slug identifier (e.g., "edrik", "milaya", "varan")
??? Visual (FNPCVisualData)
?   ??? SkeletalMesh - 3D model for the NPC
?   ??? AnimBPClass - Animation Blueprint class
?   ??? ActorScale - Scale vector (default: 1,1,1)
?   ??? NPCName - Display name
??? Audio (FNPCAudioData)
    ??? GreetingSound - Played when player approaches
    ??? InteractSound - Played during interaction
    ??? FarewellSound - Played when player leaves
    ??? IdleSounds - Array of random ambient sounds
    ??? WalkSounds - Array of walking sounds (if NPC moves)
    ??? RunSounds - Array of running sounds (if NPC runs)
```

### 3. Example DataTable Configuration

| Row Name | NPCType | SkeletalMesh | AnimBPClass | GreetingSound | InteractSound |
|----------|---------|--------------|-------------|---------------|---------------|
| edrik    | edrik   | SK_Edrik     | ABP_Edrik   | SFX_EdrikGreet| SFX_EdrikTalk |
| milaya   | milaya  | SK_Milaya    | ABP_Milaya  | SFX_MilayaHi  | SFX_MilayaTalk|
| varan    | varan   | SK_Varan     | ABP_Varan   | SFX_VaranHey  | SFX_VaranSpeak|

### 4. Server Data Mapping

The server sends NPC data with a `slug` field:
```json
{
  "id": 3,
  "name": "Edrik",
  "slug": "edrik",  // ? This maps to DataTable row
  "level": 1,
  // ... other data
}
```

The system automatically:
1. Receives NPC data from server
2. Looks up `slug` in NPCDefinitionTable
3. Loads corresponding visual/audio assets
4. Applies them to the spawned NPC

### 5. Asset Requirements

**Visual Assets:**
- `SkeletalMesh`: 3D model with rigging
- `AnimBPClass`: Animation Blueprint with idle, interaction animations

**Audio Assets:**
- `GreetingSound`: Short greeting (1-2 seconds)
- `InteractSound`: Interaction feedback (0.5-1 second)
- `FarewellSound`: Goodbye sound (1-2 seconds)
- `IdleSounds`: Array of ambient sounds (3-5 seconds each)

### 6. Troubleshooting

**Common Issues:**

1. **"NPCDefinitionTable is not set"**
   - Ensure NPCDefinitionTable is assigned in MyGameInstance
   - Check that the DataTable exists and is valid

2. **"NPC Definition not found for slug: xxx"**
   - Verify the slug exists as a row name in the DataTable
   - Check spelling matches exactly (case-sensitive)

3. **"SkeletalMesh is not set for NPC slug: xxx"**
   - Asset is not assigned in DataTable
   - Asset path is broken or asset was moved/deleted

**Debug Logging:**

The system provides detailed logging:
- `LogTemp: Warning` - Configuration issues
- `LogTemp: Log` - Successful operations
- Look for patterns like "Successfully loaded mesh for NPC"

### 7. Performance Notes

- Assets load asynchronously to prevent frame drops
- Multiple NPCs with same slug share loaded assets
- Idle sounds play randomly every 10-30 seconds
- UI components are placeholder and ready for implementation

### 8. Extension Points

The system is designed for easy extension:
- Add new audio types (combat, death, etc.)
- Implement NPC movement with walk/run sounds
- Add visual effects (particles, materials)
- Create NPC-specific interaction behaviors

## Integration with Combat System

NPCs are ready for combat integration:
- Health/Mana stats from server
- Interaction system in place
- Audio feedback for actions
- Position and rotation synchronized

The system follows SOLID principles and integrates seamlessly with existing MOB, Combat, and Network systems.