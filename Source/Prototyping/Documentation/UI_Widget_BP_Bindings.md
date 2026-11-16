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
`UItemActionRowWidget` (`Inventory/WBP_ItemActionRow`, binds `RowButton` (UButton, fill, optional — native click path when present) + `RowBackground`/`ActionLabel`/`ActionIcon`), `UDialogueChoiceButton`
(`Dialogue/WBP_DialogueChoiceButton`), `UPlayerStatsWidget` mastery rows, `UReputationWidget` rows.

## 3. Editor verification checklist (must be done in-editor, not headless)

1. `BP_MyGameInstance` defaults: every `*WidgetClass` above points at the listed BP.
2. Load-bearing optional binds — if any is missing, a Warning now logs at construct time:
   - `WBP_VendorShop`: `Buy_Confirm_Button`, `Sell_Confirm_Button` (cart dead-ends without them).
   - `W_HarverstProgress`: optional `Cancel_Button` (auto-wired to `CancelHarvest` when present; ESC always works).
   - `W_PlayerHUD`: optional `Portrait_Image` (set via `UPlayerHUD::SetPortrait`).
   - `W_PlayerInterface`: optional `BossTargetFrameWidget` (`UMobTargetFrameWidget`, same API as mob frame).
   - `Mob/WBP_MobTarget` (`UMobTargetFrameWidget`): all new binds are `BindWidgetOptional`
     and degrade gracefully, but the full design needs them in-editor —
     rank: `RankLabelText` (TextBlock), `RankBorder` (Border, frame outline);
     hp: split `MobHealthPctText` + `MobHealthValueText` (legacy combined
     `MobHealthText` still works as fallback);
     dead: `DeadOverlay` (Image, 50% over the portrait only), `DeadText` (TextBlock);
     cast: `CastBar` (ProgressBar), `CastNameText` + `CastTimeText` (TextBlocks),
     `CastSkillIcon` (Image, same DT icon as the player skill list),
     `InterruptIcon` (Image — keep, forced Collapsed until the server ships the
     interruptible flag, tracker mirenhold #134);
     effects: `StatusEffectsBox` (HorizontalBox) + set `EffectSlotClass`
     (`WBP_EffectSlot`) and `EffectDefinitionTable` on the frame instance.
     Fixed layout: no per-rank resizing, distance is not shown, no widget-wide
     desaturation by design.
   - `Inventory/W_PlayerInventorySlot` (`UInventorySlotWidget`, button-state model):
     delete the `ImgBase/ImgOccupied/ImgSelected/ImgStroke` images; `SlotButton`
     must fill the slot; set the 5 `FButtonStyle` presets
     (`Empty/Filled/Hovered/Selected/Highlighted`, priority Highlight >
     Selected > Hover > base, full `Normal/Hovered/Pressed` each);
     `RarityBorder` tints automatically, `ImgEquipped` stays the single badge.
     Unset presets keep the authored button style (never force invisible).
     `SlotButton` must fill the slot with `Visibility=Visible` and `Is Enabled`
     — otherwise the slot is dead to the cursor (this is logged as an Error).
      `SlotSizeBox` (USizeBox slot root, 64px overrides) is required, not
      optional: without it an empty button collapses to 0x0 (also an Error).
      Fill alignment is re-asserted in code (`EnforceButtonFill`), so a
      compliant BP is untouched and a broken one fails loud instead of
      silently eating clicks.
      `ImgNew` (optional UImage, corner dot/glow) marks session-new loot until
      hovered; absent = silently disabled. LMB single-selects, double-click
      acts (Use > Equip > Unequip, same dispatch as the context menu rows).
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
   RMB on a chat nickname opens the same menu for any channel (own lines
   ignored; remote senders get whisper/inspect, trade row hidden without a
   world actor). Trade needs ≤500 cm (server `economy.trade_range` re-validates); Inspect
   opens the read-only snapshot window (`playerInspect` on chunk).
7. Inventory sort/search panel (optional): `SortButton` (UButton, exact name — BindWidget matches by name) + `SearchBox` (UEditableTextBox)
   in WBP_PlayerInventory; without them the `/sort [name|rarity|type|off]` chat command works. Missing binds log a Warning at construct time.
   The Sort mode is shown in the `SortLabel` TextBlock (visible state, no hover popover). Category filter (optional): `CategoryButton` (UButton, exact name) + `CategoryLabel` (TextBlock, summary: All / name / Custom (n) from 2); the popup opens at the cursor, clamped into the viewport. Visuals: assign `CategoryFilterWidgetClass` (BP subclass of `UInventoryCategoryWidget`) with `CategoryList` (VerticalBox) + optional `CategoryFrame` (Border, never restyled by code) + `CategoryRowWidgetClass` (`UCategoryRowWidget` with `RowCheckBox` + `RowLabel` binds, alternatively picked on the inventory itself when the popup is code-built); unset = code-built default panel. Search matches base name + slug + type + the localized display name (tooltip language), both locales in one query; a missing ItemLocale table logs one Warning (DATA gap: create DT_ItemLocale, assign in DA_Localization). Weight row (optional): `WeightBar` (ProgressBar, fill+color mirror the text bucket) + `OverweightIcon` (Image, visible only at >=100%, decorative HitTestInvisible); without them text+color carry the state alone. Weight/Slots texts are numbers-only ("45.2/100", "12/48") — captions are static BP texts. Junk marks: context-menu row → dimmed slot;
   bulk sell needs `Sell_Junk_Button` in the vendor window (no-op without it).
8. Buyback: created in code when the shop opens (`Buyback_Button`, optional).
   The server keeps the last 12 sold stacks per character; the `buyback` bot scenario covers it.
9. Combat log: created in code; tunables on `UCombatLogWidget` (size/position/anchors,
   timestamps, font size, channel colours, custom row/filter widgets
   (`LogRowWidgetClass` / `LogFilterWidgetClass`), `/clearlog` in chat). RU templates are automatic.
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
Viewport-added dialogs (invite, combat log, inspect, standalone keybinds)
take their placement from the BP root's own anchors — use a full-screen
root with centered content AND set the root visibility to
SelfHitTestInvisible (children stay clickable, transparent areas pass
clicks to the world/HUD beneath — otherwise the overlay silently eats the
skillbar and blocks the camera registry). The size/position UPROPERTYs (LogSize,
LogPosition, WindowSize, …) apply to the code fallback only. The unit menu
is the exception: it auto-positions at the cursor, no designer placement.

| Window (C++ class) | WidgetClass property (owner) | Bind names |
|---|---|---|
| `UTradeInviteWidget` | `UIManager.TradeInviteWidgetClass` | `Invite_Name_Text` (Text), `Invite_Countdown_Text` (Text), `Invite_Accept_Btn` (Button), `Invite_Decline_Btn` (Button) |
| `UCombatLogWidget` (+ row/filter classes) | `UIManager.CombatLogWidgetClass`, `LogRowWidgetClass` (BP subclass of `UCombatLogRowWidget`), `LogFilterWidgetClass` (BP subclass of `UCombatLogFilterWidget`) | window: `Log_Scroll_Box` (ScrollBox), `Log_Filter_Row` (any Panel, HorizontalBox recommended); row: `Row_Icon` (Image, optional), `Row_Time_Text` / `Row_Body_Text` (Text, optional); filter: `FilterCheckBox` (CheckBox, optional), `FilterLabel_Text` (Text, optional), `FilterIcon` (Image, optional) |
| `UBuybackWidget` | — (удалён: выкуп — вкладка вендора, см. ниже) | — |
| `UVendorShopWidget` buyback tab | binds on `WBP_VendorShop`: `Buyback_Panel` (Widget), `Buyback_Items_Box` (WrapBox), `Tab_Buyback_Button` (Button); slots reuse `VendorSlotClass`, hover tooltip via shop `VendorTooltipWidget` | click slot = full-stack buyback; no [X] on buyback rows |
| `UKeybindWidget` (standalone fallback) | — (created from C++ class) | `Keybinds_List_Box` (ScrollBox), `Keybinds_Reset_Btn` (Button), `Keybinds_Close_Btn` (Button, standalone only) |
| `UKeybindWidget` (settings tab) | place as slot 3 of `WBP_Settings.TabContentSwitcher` + `Tab_ControlsButton`; set `bManageViewportPlacement=false` on the instance | same three binds, Close unbound (settings has its own close) |
| `UPlayerContextMenuWidget` | `UIManager.PlayerMenuWidgetClass` | `Menu_Title_Text` (Text, optional), `Menu_Action_List` (VerticalBox, optional), `MenuRowWidgetClass` (row design; row BP binds `MenuRow_Label`) — menu auto-positions at the cursor, no designer placement needed |
| `UPlayerInspectWidget` | `UIManager.PlayerInspectWidgetClass` | `Inspect_Title_Text` (Text), `Inspect_Gear_Box` (ScrollBox), `Inspect_Close_Btn` (Button), `DragHandle` (any UWidget, optional drag handle); window centers itself on show like the trade window; gear icons reuse the vendor slot BP (`InspectSlotClass`), hover tooltip via `InspectTooltipWidgetClass` (optional) |
| `UTradeWidget` pick + offer lists | classes on `WBP_PlayerTrade`: `TradeSlotClass` (vendor slot BP, required — all three panels; must bind `RemoveButton` or offer [X] stays hidden — watch the log warning), `TradeTooltipWidgetClass` (optional hover), `TradeQuantityPopupClass` (optional, same asset as vendor); `My/Their/Inv_Items_Box` are `WrapBox` grids; `Gold_Input` (`EditableTextBox`, required for gold — Enter/unfocus commits the offer, overdraft clamps to balance) | click pick slot = add (stacks ask qty via popup, max = remainder, equipped hidden, fully-offered rows dark); click my offer = edit qty via popup; [X] on my offer = remove; their offer display-only; lock lines show Confirmed/Pending per side (+ partner name), gold sums stay in My/Their_Gold_Text only |

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
