# UI Widget ↔ Blueprint Bindings (C++ ↔ UMG)

Source of truth for which C++ widget class is parented by which Blueprint asset,
how it is created, and which `BindWidget` names must exist in the Blueprint.
Generated 2026-10-24 from code audit (UI completeness pass, items 1/2/3/5).
Parent-class linkage must be confirmed in-editor via `BP_MyGameInstance` defaults —
uasset binaries cannot be verified headless.

## 1. UIManager-owned windows (created in `Private/UI/UIManager.*.cpp`)

| C++ class | Blueprint asset (`Content/Blueprints/UI/…`) | Created in | Toggle / visibility |
|---|---|---|---|
| `UPlayerInterfaceWidget` | `Player_HUD/W_PlayerInterface` | `CreatePlayerInterfaceWidget` | always visible (HUD root) |
| `UInventoryWidget` | `Inventory/W_PlayerInventory` | `CreateInventoryWidget` | `ToggleInventory` |
| `UHarvestProgressWidget` | `Harvest/W_HarverstProgress` (sic) | `CreateHarvestWidgets` | auto (harvest events) |
| `UHarvestLootWidget` | `Harvest/W_HarvestLootWidget` | `CreateHarvestWidgets` | auto (harvest events) |
| `UInteractionHintWidget` | `Player_HUD/WBP_InteractionHint` | `CreateInteractionHintWidget` | `Show/HideInteractionHint` |
| `UAvailableSkillsWidget` | `Skills/W_AvailableSkills` | `CreateSkillWidgets` | `ToggleSkillsPanel` |
| `UGameVersionWidget` | `System/WBP_ClientVersion` | `CreateGameVersionWidget` | always visible |
| `UGameMenuBarWidget` | `System/WBP_GameMenuBar` | `CreateGameMenuBarWidget` | always visible |
| `UGameMenuWidget` | `Menu/WBP_GameMenu` | `CreateGameMenuWidget` | `ToggleGameMenu` |
| `UIdleWarningWidget` | `System/WBP_IdleWarning` | `CreateUIWidgets` (inline) | `Show/HideIdleWarning` |
| `UDialogueWidget` | `Dialogue/WBP_DialogueWindow` | `InitializeDialogueAndQuestWidgets` | auto (NPC dialogue) |
| `UQuestJournalWidget` | `Quests/WBP_QuestJournal` | `InitializeDialogueAndQuestWidgets` | `ToggleQuestJournal` |
| `UQuestTrackerWidget` | `Quests/WBP_QuestTracker` | `InitializeDialogueAndQuestWidgets` | always visible (HUD) |
| `UEquipmentWidget` | `Equipment/WBP_PlayerEquipment` | `InitializeItemSystemWidgets` | `ToggleEquipment` |
| `UVendorShopWidget` | `Trade/WBP_VendorShop` | `InitializeItemSystemWidgets` | auto (NPC vendor) |
| `URepairShopWidget` | `Repair/WBP_RepairShop` | `InitializeItemSystemWidgets` | auto (NPC repair) |
| `UTradeWidget` | `Trade/WBP_PlayerTrade` | `InitializeItemSystemWidgets` | auto (trade invites) |
| `USkillShopWidget` | `SkillShop/WBP_SkillShop` | `InitializeSkillShopWidget` | auto (NPC skill shop) |
| `UPlayerStatsWidget` | `Stats/WBP_PlayerStats` | `InitializeStatsWidget` | `TogglePlayerStats` |
| `UTitlesWidget` | `Titles/WBP_PlayerTitles` | `InitializeTitlesWidget` | `ToggleTitles` |
| `UReputationWidget` | `Reputation/WBP_PlayerReputation` | `InitializeReputationWidget` | `ToggleReputation` |
| `UEmoteListWidget` | `Emotes/WBP_Emotes` | `InitializeEmoteListWidget` | `ToggleEmoteList` |
| `UBestiaryWidget` | `Bestiary/WB_Bestiary` | `InitializeNotificationSystem` | `ToggleBestiary` |
| `UChatWidget` | `Player_HUD/WBP_PlayerChat` | `InitializeNotificationSystem` | always visible (HUD) |
| `UNotificationToastWidget` | `Notifications/WB_NotificationToast` | `InitializeNotificationSystem` | auto (routed by `WorldNotificationManager`) |
| `UNotificationZoneBannerWidget` | `Notifications/WB_NotitficationZone` (sic) | `InitializeNotificationSystem` | auto |
| `UNotificationScreenCenterWidget` | `Notifications/WB_NotificationScreenCenter` | `InitializeNotificationSystem` | auto |
| `UNotificationAtmosphereWidget` | `Notifications/WB_NotificationAtmosphere` | `InitializeNotificationSystem` | auto |
| `UWIOInteractionPromptWidget` | `WIO/WBP_WIOInteraction` | `InitializeWIOWidgets` | `Show/HideWIOInteractionPrompt` |
| `UWIOChannelBarWidget` | `WIO/WBP_WIOChannelBar` | `InitializeWIOWidgets` | `Show/HideWIOChannelBar` |
| `UDeathScreenWidget` | `System/WBP_DeathScreen` | lazy in `ShowDeathScreen` | `Show/HideDeathScreen` |
| `UCombatScreenFlashWidget` | `Player_HUD/WBP_CombatScreenFlashWidget` | lazy in `EnsureFlashWidget` | show-only flashes |
| `UW_SettingsWidget` | `Menu/WBP_Settings` | lazy in `HandleSettingsClicked` | open via game menu; close via `Button_Close` → `OnSettingsWindowClosed` → `UIManager.HandleSettingsClosed` |

Embedded panels (not UIManager-owned):
- `UAudioSettingsWidget` → `Menu/WBP_AudioSettings`, embedded as `AudioSettingsPanel` slot in `WBP_Settings` (`W_SettingsWidget.h:133`). UIManager no longer references it (stale members removed 2026-10-24).
- `UGraphicsSettingsWidget` → `Menu/WBP_GraphicsSettings`, embedded as graphics tab.
- `ULanguageSettingsWidget` → `Menu/WBP_LanguageSettingsWidget`, embedded as language tab.

Inside `W_PlayerInterface` (not UIManager-directly): `USkillBarWidget` (`Skills/W_SkillsBar`),
`UPlayerExperienceWidget` (`Player_HUD/W_PlayerExpInfo`), `UPlayerHUD` (`Player_HUD/W_PlayerHUD`),
`UDamageCanvasWidget` (`W_DamageCanvas`), `UMobTargetFrameWidget` (`Mob/WBP_MobTarget`),
`UItemQuickBarWidget`, `UActiveEffectsWidget` (`ActiveEffects/WBP_ActiveEffects`),
`UCastBarWidget` (`Player_HUD/WBP_CastBar`), `UNameplateCanvasWidget`, `UChatBubbleWidget`
(`Player_HUD/WBP_ChatBuble`, `WBP_ChatBubleNPC`).

## 2. Row/slot child classes (created in code, BP row asset assigned per window)

`UInventorySlotWidget`, `USkillSlotWidget`, `USkillItemWidget`, `UVendorSlotWidget`,
`URepairShopRowWidget`, `USkillShopRowWidget`, `UTitleRowWidget` (`Titles/WBP_PlayerTitleRow`),
`UBestiaryTierRowWidget`/`UBestiaryMobRowWidget`/`UBestiaryEntryWidget`
(`Bestiary/WB_TierRow`, `WB_MobRow`, `WB_BestiaryEntry`), `UQuestRewardRowWidget`
(`Quests/WBP_RewardItem`), `UEmoteItemWidget` (`Emotes/WBP_EmoteItem`),
`UHarvestLootItemWidget` (`Harvest/W_HarvestLootItem`), `UChatMessageLineWidget`
(`Player_HUD/WBP_PlayerChatLine`), `UItemActionMenuWidget` (`Inventory/WBP_Item_Actions`),
`UItemActionRowWidget` (`Inventory/WBP_ItemActionRow`), `UDialogueChoiceButton`
(`Dialogue/WBP_DialogueChoiceButton`), `UPlayerStatsWidget` mastery rows, `UReputationWidget` rows.

## 3. Editor verification checklist (must be done in-editor, not headless)

1. `BP_MyGameInstance` defaults: every `*WidgetClass` above points at the listed BP.
2. Load-bearing optional binds — if any is missing, a Warning now logs at construct time:
   - `WBP_VendorShop`: `Buy_Confirm_Button`, `Sell_Confirm_Button` (cart dead-ends without them).
   - `W_HarverstProgress`: optional `Cancel_Button` (auto-wired to `CancelHarvest` when present; ESC always works).
   - `W_PlayerHUD`: optional `Portrait_Image` (set via `UPlayerHUD::SetPortrait`).
   - `W_PlayerInterface`: optional `BossTargetFrameWidget` (`UMobTargetFrameWidget`, same API as mob frame).
3. Multi-client run (`LaunchClients.ps1 -n 3`): open/close every toggle window, log-scan for
   `BindWidget`, `Failed to find`, `Accessed None` — zero hits is the gate.
4. Equipment window: hover a slot → item tooltip follows the cursor (bound
   `EquipTooltipWidget` or `EquipTooltipWidgetClass` detail, else a Warning logs);
   RMB an occupied slot → context menu (Unequip / Use), never instant unequip.
   `EquipContextMenuWidgetClass` must be set (else a Warning logs and RMB is a no-op).
5. Item quickbar: assign via inventory/equipment context menu → `Quickbar`
   (usable items only); hotkeys `QuickSlot1..5Action` (suggest F6-F10 — F1 is
   TargetSelf, 1-5 drive the skill bar); per-character persistence is automatic.
6. Unit menu: RMB-click (no drag) another player → Whisper / Trade / Inspect.
   Trade needs ≤500 cm (server `economy.trade_range` re-validates); Inspect
   opens the read-only snapshot window (`playerInspect` on chunk).
7. Inventory sort/search panel (optional): `Sort_Button` (UButton) + `SearchBox` (UEditableTextBox)
   in WBP_PlayerInventory; without them the `/sort [name|rarity|type|off]` chat command works.
   The Sort button tooltip shows the active mode. Junk marks: context-menu row → dimmed slot;
   bulk sell needs `Sell_Junk_Button` in the vendor window (no-op without it).
8. Buyback: created in code when the shop opens (`Buyback_Button`, optional).
   The server keeps the last 12 sold stacks per character; the `buyback` bot scenario covers it.
9. Combat log: created in code; tunables on `UCombatLogWidget` (size/position/anchors,
   timestamps, font size, channel colours, `/clearlog` in chat). RU templates are automatic.
10. Keybinds: window created in code; open via `IA_Keybinds` (suggest F12) → `KeybindsAction`
    in the player Blueprint + IMC_Player. Persisted to Game.ini, Reset all inside the window.
11. Player target frame: reuses the mob frame (name/level/HP), auto-hides when the
    target leaves the world, 0 HP on death — all in code, no BP needed.

## 6. Code-built windows: Blueprint bind names

Windows below build their layout in C++ and work with zero BP setup.
To restyle one: create a BP subclass, bind the listed names, assign the
subclass to the `*WidgetClass` property. Bind all names of a window or
none — rows inside lists are always built in code (uniform styling).
Unassigned `*WidgetClass` = code fallback (headless-safe, bots cover it).

| Window (C++ class) | WidgetClass property (owner) | Bind names |
|---|---|---|
| `UTradeInviteWidget` | `UIManager.TradeInviteWidgetClass` | `Invite_Name_Text` (Text), `Invite_Countdown_Text` (Text), `Invite_Accept_Btn` (Button), `Invite_Decline_Btn` (Button) |
| `UCombatLogWidget` | `UIManager.CombatLogWidgetClass` | `Log_Scroll_Box` (ScrollBox), `Log_Filter_Row` (any Panel, HorizontalBox recommended — channel checks are built into it) |
| `UBuybackWidget` | `UVendorShopWidget.BuybackWidgetClass` | `Buyback_Title_Text` (Text), `Buyback_List_Box` (ScrollBox), `Buyback_Close_Btn` (Button) |
| `UKeybindWidget` (standalone fallback) | — (created from C++ class) | `Keybinds_List_Box` (ScrollBox), `Keybinds_Reset_Btn` (Button), `Keybinds_Close_Btn` (Button, standalone only) |
| `UKeybindWidget` (settings tab) | place as slot 3 of `WBP_Settings.TabContentSwitcher` + `Tab_ControlsButton`; set `bManageViewportPlacement=false` on the instance | same three binds, Close unbound (settings has its own close) |
| `UPlayerContextMenuWidget` | `UIManager.PlayerMenuWidgetClass` | `Menu_Title_Text` (Text, optional), `Menu_Action_List` (VerticalBox, optional), `MenuRowWidgetClass` (row design; row BP binds `MenuRow_Label`) |
| `UPlayerInspectWidget` | `UIManager.PlayerInspectWidgetClass` | `Inspect_Title_Text` (Text), `Inspect_Gear_Box` (ScrollBox), `Inspect_Close_Btn` (Button); gear icons reuse the vendor slot BP (`InspectSlotClass`), hover tooltip via `InspectTooltipWidgetClass` (optional) |

Settings tab order is index-bound (`ESettingsTab`: Audio=0, Graphics=1,
Language=2, Controls=3) — new tabs go last, never reorder.

## 4. Removed 2026-10-24 (do not recreate)

- `Public/UI/ItemContextMenuWidget.h` + `Private/UI/ItemContextMenuWidget.cpp` (0 bytes, superseded by `ItemActionMenuWidget`).
- `Public/UI/SkillSlotWidget.h.bak` (stale backup).
- Stale `UIManager` members: `AudioSettingsWidgetClass`, `AudioSettingsWidget` instance,
  `GetAudioSettingsWidget()`, `HandleAudioSettingsClosed()` (+ 11 unused includes).
  `UAudioSettingsWidget` class itself is live (settings tab panel).

## 5. Deliberately NOT changed (verified working, see code refs)

- `WorldNotificationManager::SubscribeToNetworkEvents[_Early]` — idempotent (`bIsSubscribed` guard + `RemoveDynamic` before `AddDynamic`).
- `USkillBarWidget::OnSkillCast`, `UEmoteItemWidget::OnEmoteItemHovered`,
  `UEquipmentWidget::OnEquipSlotLeftClicked` — documented `BlueprintAssignable` extension points (casting/unequip-request flow itself is C++-complete).
- `UGraphicsSettingsWidget::CloseWidget` — invoked by parent `UW_SettingsWidget::CloseSettings` (no per-tab close button by design).
- `UBestiaryNetworkHandler` — owns cache + pending-overview replay like a manager; no wrapper introduced (would be pure indirection).
- `UMasteryManager` data is fully displayed in `PlayerStatsWidget` mastery section; a dedicated tree window needs game-design + BP work (editor task).
- `UQuestManager::OnQuestFailed` now re-broadcasts `OnQuestUpdated` so journal/tracker refresh the red `failed` state immediately.
