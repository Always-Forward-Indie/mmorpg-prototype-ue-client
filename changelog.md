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

===========

v0.0.9a
16.06.2026
===========

2026-06-16
----------
Refactored loading screen system: replaced GameViewportClient widget approach with MoviePlayer-based loading screen that survives CleanupWorld/OpenLevel without black frames or flickering.
Fixed login UI flashing during game world transition by guarding OnPostLoginLevelLoaded with bTransitioningToGameWorld check.
Added texture streaming gate (Gate 5) to game world readiness checks to prevent mip pop-in.
Improved loading screen safety timer: now only removes loading screen if world is actually ready.
Optimized HasCursorOverWindowContent() with early return when no UI windows are open; added SlateApplication validity guard.
Added IsValid() guard on UIManager in BasicPlayer::IsUIBlockingInteraction().
Added PCG component auto-regeneration on graphics settings apply to update grass/landscape at new quality.
Added PCG, MoviePlayer, RenderCore module dependencies to Build.cs.
Updated server IPs in server_config.json.

===========

v0.0.10a
17.06.2026
===========

2026-06-17
----------
Added Idle Timeout system: UIdleTimeoutManager tracks input inactivity with configurable timeout (default 300s) and warning (60s); auto-disconnects on AFK. IdleWarningWidget (WBP_IdleWarning) shows countdown overlay.
Added client version checking: ClientVersion field (SemVer) sent in login/register requests; version mismatch popup on ERR_VERSION_OUTDATED / ERR_VERSION_TOO_NEW with Quit option.
Added Social Links widget (USocialLinksWidget, WBP_SocialLinks) on login screen: Telegram, Website, Twitter, Youtube, Discord buttons.
Added Bug Report button on game menu (WBP_GameMenu) and login overlay (WBP_LoginScreenOverlay); opens configurable URL.
Extracted FEntityAudioProfile and FEntitySkillVoiceOverride from DataStructs.h into EntityAudioData.h; added FootwearType, Footsteps (unified array), NPC social sounds (GreetingSound, InteractSound, FarewellSound), DefaultAttenuation.
Added DefaultAttenuation support across all entity audio: Player, MOB, NPC, projectiles, dropped items, footstep notifies. All sound spawns now use attenuation from entity's audio profile.
Unified WalkSounds/RunSounds into single FootstepSounds array across MOB, NPC, and FEntityAudioProfile. Footstep surface lookup uses composite key PhysMat_FootwearType (e.g. PM_Stone_boot) with generic fallback.
Removed Walk/Run slots from AnimNotify_PlaySoundFromTable; footstep logic now handled exclusively by AnimNotify_Footstep with entity pool fallback (Player -> FEntityAudioProfile, MOB/NPC -> FootstepSounds array).
Added item audio: EquipSound, UnequipSound, UseSound, DefaultAttenuation to FItemVisualData. EquipmentManager plays equip/unequip sounds; InventoryManager plays item use sound; DroppedItemActor uses attenuation on pickup/drop sounds.
Refactored data structs: removed redundant MobType/NPCType fields (row name is slug); added AudioProfileId to FMobDefinition and FNPCDefinition for entity-level audio profile lookup.
BasicPlayer: AudioProfileId defaults to None (set from DT_CharacterVisuals at spawn); caches FootwearType from audio profile; other-client nameplate HP updates; chat input focus released on camera rotation.
BasicMOB: unified FootstepSounds array; removed PlayWalkRandomSound/PlayRunRandomSound; DefaultAttenuation and FootwearType fields from audio profile.
BasicNPC: AudioProfileId priority chain for audio (Profile -> Legacy FNPCAudioData); unified FootstepSounds; DefaultAttenuation and FootwearType fields; GetFootstepSounds/GetFootwearType accessors.
Added FCT distance culling: MaxVisibleDistanceCm and FadeStartDistanceCm configurable in UIManager; cached local Pawn for distance checks; ShowDamage/ShowSpecialText skip distant targets.
Chat widget: auto-refocus input box after Enter submit; ReleaseInputFocus() for returning keyboard control to game.
Replaced W_Version.uasset with typed WBP_ClientVersion.uasset (UGameVersionWidget C++ backing class).
Added PCG component suppression during World Partition streaming to prevent UnrealEditor-PCG.dll access violation when landscape data is not fully loaded. Periodic sweep catches newly streamed cells.
Deferred PCG activation 1s after Gate 3 to allow landscape heightfield GPU resources to finalize. ActivateAllPCGComponents/SuppressPCGComponents on UMyGameInstance.
Added version field to FMessageDataStruct for protocol versioning. JSONParser deserializes version from network header.
Updated server IPs to 127.0.0.1 in server_config.json.
New audio assets: SA_Default, SA_Steps, SFX/ content directory.

===========

v0.1.1
19.06.2026
===========

2026-06-19
----------
Upgraded engine from UE 5.7 to UE 5.8: .uproject, Build.cs (added Landscape module), LaunchClients.ps1/bat, AGENTS.md. Updated server IPs to remote host (23.88.102.182).
Added CrashDiagnostics system (CrashDiagnostics.h/.cpp): CRASH_GUARD(Name) macro with RAII global context pointer for crash dump attribution. Wrapped all Tick() and packet-processing entry points in Player, MOB, NPC, Item, Harvest, NetworkManager.
Networking: fixed disconnect shutdown order — stop workers before DestroySocket to prevent use-after-free purecall crashes. Replaced FSocket* and bRunThread with std::atomic in NetworkReceiverWorker/NetworkSenderWorker for hardware-level thread safety. Added UPOPERTY() on critical members in PingManager, AuthManager, PlayerManager.
Player: added 5-second respawn grace window — ignores healthCurrent=0 packets arriving shortly after revive. Dead players now reject positive HP updates to prevent corpse HP bars. Footstep sounds preloaded asynchronously via FStreamableManager to eliminate LoadSynchronous hitches.
Animation/Footstep 2.0: increased trace distance 50→200 for reliable ground detection. Priority chain: physmat+footwear → physmat → default sound → entity fallback pool. Added console variable footstep.Debug for runtime resolution debugging. Fixed AnimNotifyState_WeaponTrail RegisterComponent order (after AttachToComponent).
NPCs: added ClearWorldState() to NPCManager — removes all spawned NPCs and pending spawn queue on level transition. NPC cleanup timer now invalidated before world pointer change.
Items: ItemManager buffers network events in PendingNetworkEvents when worldContext is null, replays them via FlushPendingEvents() in the new world. ClearWorldState() flushes all item state on level transition.
Mobs: added CRASH_GUARD to all Tick() and packet-processing loops. Null-world early-return guards in MOBManager and SpawnZoneManager.
HarvestManager: StopTicking + TickTimerHandle invalidation now always called before world pointer change.
DroppedItemActor: PlayPickupEffect() changed Destroy() → Hide+DisableCollision+LifeSpan(0.5s) to prevent use-after-destroy crashes from overlap queries.
Equipment: RegisterComponent moved after AttachToComponent for both Static and Skeletal mesh paths, fixing one-frame equip position glitch. EquipmentNetworkHandler: fixed dangling reference in slot key iteration.
GameInstance: removed all PCG suppression/activation code (SuppressPCGComponents, ActivateAllPCGComponents, PCGSuppressionTimerHandle) — no longer needed on UE 5.8. RemovePlayerData() now hides and disables mesh/collision before Destroy to safely deregister from Nanite scene render pipeline. Footstep sound preloading added to Init(). Shutdown cleans up EndFrameDelegateHandle and GTRemoveTicker. Added PIE/editor code paths for loading screen in WITH_EDITOR.
TimeSync: GetSystemTimeMs() rewritten from thread-unsafe static locals to member variables protected by FCriticalSection with double-checked locking. Eliminates rare timestamp corruption from concurrent game + network threads.
AudioManager: Invalidated TrackEndTimerHandle before world change to prevent crash on level transition while audio is playing.
UI: GraphicsSettingsWidget defers ApplySettings via 0.1s timer and calls IStreamingManager::NotifyLevelChange() after apply to prevent landscape material rebuild crash. PCG refresh checks IsGameWorldReady() before regenerating. SkillSlotWidget uses IsValid() in BeginDestroy to avoid calling RemoveDynamic on garbage-collected UObject. FCTManager: added UPOPERTY() on RootCanvas and PlayerController. TargetDecalComponent: fixed RegisterComponent order.
JSONParser: added HasField() checks before all DeserializeCharacterData() field lookups to prevent crashes on partial/missing server data.
Bestiary/WorldNotification/DevMode: replaced TMap::Add() with Emplace() for more efficient in-place construction.
Config: DefaultEngine.ini — added r.SetRes=1920x1080w and LogCrashDiag=NoLogging. DefaultEditor.ini — added editor preview viewport profiles.
New assets: UI click sounds (button_click_v1, small_button_click_v1). Reed foliage meshes (reed_1/2/3). Updated ~120 WorldMapV1 external actor cells, data tables (Items, Footsteps, Skills, Dialogue, Quests), and ~30 UI widget Blueprints.

===========

v0.1.2
19.06.2026
===========

2026-06-19
----------
Localization: added multi-language support. ULocalizationSubsystem now reads persisted locale from GameUserSettings.ini on Initialize(), defaults to "en". SetLocale() method switches language DataAsset, persists choice, broadcasts OnLocaleChanged for open widget refresh. Added separate DataAssets for RU (LocalizationDataAssetRU) and EN (LocalizationDataAssetEN) on GameInstance.
Language Settings: new ULanguageSettingsWidget with ComboBox (en/ru) in the Settings window. ESettingsTab::Language tab added with auto-apply on selection change or Apply button.
Quest Tracker: added bTracked field to FQuestProgressData (default true). QuestManager: SetQuestTracked(), GetTrackedQuests() for toggle logic. OnQuestUpdated() preserves bTracked from existing data. QuestTrackerWidget now shows only tracked (not all active) quests, hides when none tracked. QuestJournalWidget: new Quest_Row_Track button per row with UQuestTrackBinding helper.
GameMenuBar: refactored button click handlers into compact inline delegates. Added UHintTooltipWidget — simple tooltip with Hint_Text TextBlock, assigned via native SetToolTip mechanism. SetupButtonHint() creates tooltip for each of 10 bar buttons with configurable HintText properties.
Free-Look Camera: bAltCursorActive now defaults to false (cursor starts hidden). Look() supports free-look mode when cursor hidden AND no UI window open — camera rotates and character follows yaw without holding RMB. In free-look, character also rotates toward camera facing direction.
Music Zones: two-layer guard against spurious EndOverlap events during Actor spawn/Possess/capsule resize. Layer 1: if active playlist doesn't match this zone, don't interfere. Layer 2: IsPlayerInAnyMusicZone() — don't stop music if player overlaps ANY other music zone. Physics overlap check via GetOverlappingActors() confirms pawn is genuinely outside. LastActivePlaylistId preserved when StopMusic() is called so SetMusicVolume(0→>0) can resume the playlist.
Ambient Sound Zones: static ref-count system (ActiveAmbientRefCount TMap<USoundBase*, int32>) tracks concurrent zone coverage. StartAmbient() increments count instead of restarting if sound already playing. StopAmbient() only actually stops when refcount reaches 0 (last zone left). ActiveAmbientComponents TMap tracks which AudioComponent started playback. EndPlay() cleanup for static maps on zone destruction.
AudioManager: PushSoundMixModifier now called only when transitioning volume 0→>0, not on every slider tick (prevents audio device modifier stack instability). SetMusicVolume(0→>0) resumes playlist via LastActivePlaylistId. PlayPlaylist() restarts if matching playlist's component stopped playing. SafeVolume changed from KINDA_SMALL_NUMBER to 0.005f.
Item Pickup Queue: PendingPickupItemUID/Item replaced with PendingPickupItemUIDs/Items (TArray). OnPickupPointFired() resolves multiple rapid pickups in one animation fire. PickupSpecificItem(): IsPickingUp() guard prevents overlapping pickup animations from corrupting queues.
Net Subscription Order: InventoryManager and HarvestManager net subscriptions deferred from InitNetworkingSetup() to BasicPlayer::BeginPlay(), preventing packet processing before OwnerCharacterId is set.
Overlap Delegates: RemoveDynamic called before AddDynamic on 5 actors (BaseMMOProjectile, MobSpawnZone, WorldInteractiveObjectActor, AAmbientSoundZoneActor, AMusicZoneActor) to prevent double-binding from Blueprint recompilation or repeated BeginPlay.
QuestJournal: UQuestTrackBinding helper class for tracking toggle button. Blueprint row now includes Quest_Row_Track button visible only for active quests.
New Blueprint widgets: WBP_LanguageSettingsWidget (language selection combo), WBP_HintTooltipWidget (button hover hints). Updated BP_MyGameInstance (LocalizationDataAssets RU/EN), WBP_Settings (Language tab), WBP_QuestJournalRow (track button), WBP_GameMenuBar (hint tooltips).

==========

v0.1.3
26.06.2026
===========

2026-06-24
----------
Localization live-refresh: 13 widgets (VendorShop, Trade, BestiaryEntry, Dialogue, QuestJournal, QuestTracker, RepairShop, MobTargetFrame, NPCNameplate, WIOInteractionPrompt, SkillShop, BasicMOB, LanguageSettings) now subscribe to OnLocaleChanged and refresh automatically on language switch. NativeConstruct/NativeDestruct pair ensures proper cleanup.
Localization subsystem: Initialize() no longer sets locale prematurely (DataAssets not yet assigned). SetLocale() now calls FInternationalization::SetCurrentCulture() for engine-level culture sync. Locale re-initialized in GameInstance::Init() after DataAsset assignment. Error log if all DataAssets are NULL.
EN translations completed: 636→1954 words translated, full parity with RU. Localization gather paths extended to DataTables/ and Blueprints/UI/.
Interaction Hint Widget: new UInteractionHintWidget with WBP_InteractionHint Blueprint. Shows "Press F to..." prompts on hover (via CursorInteractionComponent::OnHoverChanged) and proximity (nearby interactables without direct cursor over). Created by UIManager, controlled via ShowInteractionHint/HideInteractionHint/ShowProximityHint/ClearProximityHint.
Proximity hint scanning in BasicPlayer::UpdateProximityHint(): scanned every 0.25s, finds nearest interactable (harvestable corpse, harvested corpse, dropped item, NPC) within their respective ranges and shows hint. Hover hint takes priority over proximity.
Unified Interact key: removed dedicated PickupAction and HarvestAction input bindings from BasicPlayer. OnInteractInput() (F key) now contextually dispatches: harvestable corpse → NPC dialogue → pickup item. OnPickupInput() and OnHarvestInput() removed.
Non-combat approach persistence: harvest, NPC talk, and item pickup approaches no longer interrupted by WASD movement or mouse-button forward — only combat auto-attack is cancelled. Uses bWasNonCombat pattern in UpdateApproach, TryCastSkillWithApproach, Move, and HandleMouseButtonsMoveForward.
Harvest improvements: KnownEmptyCorpses TSet tracks server-confirmed empty corpses — GetNearestHarvestedCorpse skips them, HandleHarvestComplete and HandleLootPickupResponse populate the set, BasicMOB::EndPlay removes from set on cleanup. corpseRemoved server event also clears from set. Chunk server connection guard before StartHarvest (shows on-screen debug message if disconnected). MaxHarvestDistance reduced 300→180 to match server validation radius. Approach uses GetMaxHarvestDistance() for harvest, GetItemPickupRange() for pickup — separated from generic InteractionRange.
Pickup refinements: ItemPickupRange config (180cm default) added to WorldInteractionConfig, exposed via CursorInteractionComponent::GetItemPickupRange(). DroppedItem PickupRadius changed 200→180. canBePickedUp set to false immediately on pickup attempt; re-enabled on server rejection. GetNearestDroppedItem fix: ClosestDistance init changed from MaxDistance to MaxDistance+1 so exact-range items are found.
Item pickup toast: ProcessItemPickup() enqueues "item_received" toast via WorldNotificationManager with localized item name. Toast fallback reads item_slug field alongside itemSlug.
MOB combat animation: hit-react now interrupts active attack montage (Montage_StopGroupByName) and clears attack state before starting hit animation — hit takes priority over attack. Hit-react also triggered from DamageEffectHandler::ProcessSkillResult for mob targets (was only in BasicMOB::OnReceiveSkillResult). Debug logging added for hit-react traceability.
Mana guards: finalTargetMana and finalCasterMana checks changed from >= 0 to > 0 to prevent zeroing mana on non-mana skills. Applied in CombatSystemManager, DamageEffectHandler, and BasicMOB.
MOB movement: GroundTraceDepth increased 800→5000 to handle tall structures. Downward ground interpolation speed now scaled by Z-difference factor (6× at 240u drop, 1× at <40u) for responsive descent from elevated surfaces.
Loading screen hardening: 8-second hard safety timer on game world load forces removal if gate pipeline stalls. 3-second wall-clock safety net on frame countdown (render thread may stall when window unfocused). MoviePlayer null-guard and reconfiguration in CheckAllReadyFlags for packaged builds. bAllowEngineTick = true in SetupMoviePlayerLoadingScreen to keep timers running during load. Window force-to-front (BringToFront + SetWindowFocus) on transition to game world. Viewport focus restored via SetAllUserFocusToGameViewport after loading screen removal.
Skill system: skillSlug field removed from FSkillDefinitionData — row name in DT_Skills is now the canonical slug. SkillDefinitionRepository::LoadDefinitionsFromTable uses row name directly. Removed redundant skillSlug assignments in PlayerSkillManager and SkillDefinitionRepository. SkillShop: PopulateSkillRows now loads icons from SkillDefinitionRepository and overrides skill name with localized displayName from definition. Debug logging added for icon/name resolution.
Tooltip & Drag: SkillTooltipWidget::UpdateTooltipPosition rewritten — uses viewport-space mouse position (GetMousePositionOnViewport), ForceLayoutPrepass with GetCachedGeometry/GetDesiredSize fallback chain, proper screen-edge clamping, SetAlignmentInViewport(0,0) for stable positioning. TooltipOffset changed (10,-10)→(20,20). BestiaryEntryWidget inherits from UFocusableWindowWidget (was UUserWidget) with full drag support (NativeOnMouseButtonDown/Up/Move, DragHandle, auto-centering on standalone open). DragThresholdPixels increased 8→20.
VendorShop UX improvements: optimistic UI on sell — SellCart cleared immediately before server response. Server-authoritative gold credited on sell batch result (CachedInventory.gold += Result.totalGoldReceived). Conditional inventory deduction on sell batch — only subtracts if item still exists (getPlayerInventory may have already replaced cache). Localized item names in buy/sell status messages via ResolveItemNameFromShop/ResolveItemNameFromInventory helpers. RefreshShopDisplay on locale change.
HarvestProgressWidget: FSlateApplication::SetAllUserFocusToGameViewport() called on hide to restore input focus. AvailableSkillsWidget: bIsShowingTooltip initialized to false. LanguageSettingsWidget: InitializeSettings() called in NativeConstruct.
Config: server_config.json back to localhost (127.0.0.1). DefaultGame.ini: Prerequisites=False for packaging. DefaultEditor.ini: localization engine targets updated with MetadataFieldOuterTypes fields. Config/Localization/Game_Gather.ini: gather paths extended to Content/DataTables/* and Content/Blueprints/UI/*.
Data assets: DT_Skills expanded (8→26 KB). ItemsData, MobsData, DT_Effects, DT_Emotes updated. BP_MyGameInstance, WBP_LanguageSettingsWidget, WBP_GameMenuBar, WBP_MobTarget, WBP_PlayerNameplate, WBP_EmoteItem, WBP_Emotes, W_SkillSlot Blueprint updates. WorldMapV1 cell data updated.

===========