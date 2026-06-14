v0.0.2a
28.02.2026
===========
Improved logic of Time Sync GetSystemTimeMs logic to prevent issues with non correct calcualtions.
Fixed issue with not correct mystic random value for rollback timeout for skill on skills panel.
Fixed issue with stats and exp, lvl update on all already connected clients when some new client connected.
Fixed issue with widget update on all connected client when new one connecting.
Fixed correctly update and calculation for exp UI bar.
===========

v0.0.6a
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

v0.0.7a
11.06.2026
===========

Imrpoved and fixed networking and ping logic.
Fixed and improved character delete logic and UI.
Improved loading screen logic.
Improved other client player movement and animation to make it smoother and more responsive.

===========

v0.0.8a
14.06.2026
===========

2026-06-13
----------
Added jump system for local player: OnJumpPressed/OnJumpReleased bound to JumpAction, CMC JumpZVelocity 420, AirControl 0.35, JumpMaxHoldTime 0.2s.
Added bIsFalling field to FCharacterDataStruct; serialized in JSONParser and sent to server with move requests.
Server syncs isFalling to remote clients; PlayerAnimInstance reads bIsInAir from server for remote players and from CMC for local player.
Overhauled remote player movement: dead reckoning (open-loop velocity integration) replaced linear lerp. Velocity recalculated from consecutive server positions each packet. Gap-correction term closes actor-to-target lag without per-frame SetActorLocation jerking. Z-axis interpolated separately for natural stair/rock traversal.
Added PositionBlendStrength (0.2) to control gap-correction strength; negative gap (jitter overshoot) lowers velocity without back-step.
Added RemoteVelocity, bRemoteIsInAir fields to BasicPlayer; GetRemoteIsInAir() accessor for anim instance.
Added ClearLockedTargetOnAllPlayers() to MOBManager — clears target on all spawned players when a mob dies or its corpse is removed.
Added 2-second cooldown on emote sending to prevent spam (EmoteNetworkHandler).

2026-06-14
----------
Login flow UX improvements: Enter key triggers primary action on all panels (Login, Register, CharSelect, CharCreate, Play). Text field OnTextCommitted also triggers login/register on Enter. Keyboard focus set on invalid fields after validation errors.
Added NativeOnPreviewKeyDown override to intercept Enter before ListView consumes it on CharacterSelect panel.
Loading screen now created in GameInstance::Init() so default map is never visible. OpenLevel deferred by one tick (FTSTicker) in LoadLevel, ReturnToLoginLevel, and TransitionToGameWorld to ensure Slate composites the loading screen first.
Added HasCursorOverWindowContent() to UIManager — detects if cursor is over a Visible child widget of any open UI window (recursive tree walk). SelfHitTestInvisible areas pass through.
BasicPlayer::IsUIBlockingInteraction() now uses HasCursorOverWindowContent() instead of always returning false. Look() (camera rotation) blocked when cursor is over UI content.
Added IsValid() guards on all LockedTarget accesses to prevent stale-pointer crashes (BasicPlayer).
QuantityPopupWidget and DropQuantityPopupWidget: click-outside-closes behavior via NativeOnMouseButtonDown; full-viewport anchors for proper click-outside detection.
Capsule size fix: SetCapsuleRadius now uses BoxExtent/2 instead of full BoxExtent.
GameDefaultMap changed to /Game/Maps/MainGameContainerLevel. Added server_config.json to .gitignore.
PCG grass assets pruned (LGT_Grass1_Runtime, LGT_Grass_Runtime deleted; PCG_Grass rebuilt). World map cell data updated.