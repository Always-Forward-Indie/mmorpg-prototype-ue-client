v0.0.2a
28.02.2026
===========
Improved logic of Time Sync GetSystemTimeMs logic to prevent issues with non correct calcualtions.
Fixed issue with not correct mystic random value for rollback timeout for skill on skills panel.
Fixed issue with stats and exp, lvl update on all already connected clients when some new client connected.
Fixed issue with widget update on all connected client when new one connecting.
Fixed correctly update and calculation for exp UI bar.
===========

v0.0.3a
11.06.2026
===========

2026-04-04
----------
Refactored data server sync layer: AuthenticationManager, PlayerManager, NetworkManager, GameInstance communication.
Refactored GameInstance initialization and PlayerStatsNetworkHandler.
Added CombatCameraShake and CombatScreenFlashWidget; improved combat system, items, battle system, VFX, dropped items; expanded ItemManager and combat logic.
Improved Sound System: AudioManager, MusicZoneActor, footstep notifications, AnimNotify_PlaySoundFromTable; added SkillDefinitionRepository.
Expanded skill slot widget with drag-and-drop support; improved AvailableSkillsWidget and SkillBarWidget layout.
Improved skill bar: skill slot logic, drag-and-drop operation flow.

2026-04-05
----------
Added UI nameplates system and manager: PlayerNameplateComponent, NameplateCanvasWidget, nameplate widget logic.
Added UI notifications system and controller.
Refactored mob movement logic (MOBMovementComponent core rewrite).

2026-04-06
----------
Worked on skills system shop: UI, logic, and server communication for skill purchases.
Improved mob movement: MovementComponent path-following and waypoint logic.
Improved mob movement: MOBManager synchronization and MovementComponent performance.

2026-04-08
----------
Added Reputation system and ReputationWidget; fixes and improvements for existing systems.

2026-04-09
----------
Major UI overhaul across all widgets: Inventory, Equipment, VendorShop, Trade, SkillBar, SkillShop, RepairShop, Nameplates, ChatBubble, DamageText, Dialogue, EmoteList, Bestiary, Reputation, Titles, QuestJournal, HarvestLoot; added CombatSystemManager healing handler; added PlayerStatsNetworkHandler and TitleNetworkHandler improvements.

2026-04-10
----------
Improved MOB movement component and head info widgets; refactored PlayerSkillManager; improved BasicPlayer and BasicMOB logic.
Added NPC ambient sound system (AnimNotify_NPCSound); expanded BasicNPC with dialogue trigger logic; improved NPCAnimInstance and DroppedItemActor.

2026-04-11
----------
Improved NPCs, dropped items, active effects UI; added NPCAnimInstance class.
Updated data tables (effects, world interaction); world map asset updates.
Refactored BasicPlayer and BasicMOB logic; improved WorldInteractionConfig.
Added CursorInteractionComponent, TargetDecalComponent, and WorldInteractionConfig systems; expanded HarvestManager, InventoryManager, and BasicPlayer interaction logic.

2026-04-13
----------
Added emotes system with UI; added server-side sync for skill bar items; other fixes and improvements.

2026-04-14
----------
Added Features Setup Guide documentation; improved combat system and healing effects; expanded inventory, mob, NPC, player stats, skill shop, repair shop, damage text, and nameplate logic.

2026-04-15
----------
Improved MOB movement component, NPCs, player skills, mob head info UI, and player experience widget.

2026-04-16
----------
Added NPC ambient messages system; general bugfixes and improvements.

2026-04-18
----------
Added WIO (World Interaction Object) system: WIOChannelBar, WIOInteraction widgets, data table, and game logic.

2026-04-23
----------
Added login flow and character select system: LoginFlowWidget, CharacterListItem, LoginLevelSetupActor, character visual/cosmetic data; major authentication and login level rework.
Added ambient creature system: AmbientCreatureActor, AmbientBehaviors, AmbientAnimInstance, AmbientScheduleAsset (deer and rabbit with forest path assets); added CosmeticVisualComponent, CharacterPreviewManager, W_SettingsWidget, WorldMapExporter plugin; improved dialogue, repair shop, chat bubbles, WIO channel; added localization tables and ambient speech lines.

2026-06-08
----------
Refactored code structure for improved readability and maintainability. Fixed target decal rendering. Fixed audio logic and zone triggers. Fixed crash on startup. Added and updated localization strings. Fixed world map image exporter plugin.

2026-06-09
----------
Fixed combat logic issues: corrected floating damage/restore numbers. Added character titles localization support. Added usable items panel widget. Fixed shop trade button logic. Improved player HUD (HP/MP bars rendering and updates). Improved target widget behavior.
Refactored loading level logic and transitions.

===========

