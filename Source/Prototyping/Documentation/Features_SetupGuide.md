# Руководство по настройке новых фич

## Фича 1 — Счётчик убийств в тултипе оружия

### Что было сделано
В виджет тултипа предмета добавлен вывод количества убийств из поля `killCount` структуры `FItemInstance`.

### Настройка
1. Откройте Blueprint виджета `WBP_ItemTooltip` (или аналог).
2. Убедитесь, что в нём есть `TextBlock` с именем **`KillCountText`** (BindWidget).
3. Поле заполняется автоматически при инициализации тултипа через `ItemTooltipWidget::InitTooltip`.
4. Если `killCount == 0` — строка скрыта (Collapsed).

---

## Фича 2 — Тайтл игрока в неймплейте

### Что было сделано
- Добавлен метод `UW_PlayerNameplateWidget::SetTitle(FString)` — устанавливает текст тайтла, скрывает строку при пустой строке.
- Добавлен `UNameplateManager::SetPlayerTitle` → `UNameplateCanvasWidget::SetPlayerTitle` → `Nameplate->SetTitle()`.
- Добавлен `UPlayerNameplateComponent::UpdateTitle(FString)` — точка входа для кода вне UI.
- Локальный игрок автоматически обновляет свой тайтл в неймплейте при экипировке через подписку `UTitleManager::OnTitlesUpdated`.
- `ABasicPlayer::SetEquippedTitle(FString)` — публичный BlueprintCallable-метод для установки тайтла удалённым игрокам.

### Настройка Blueprint
1. Откройте `WBP_PlayerNameplate` (виджет отдельного неймплейта игрока).
2. Добавьте `TextBlock` с именем **`TitleText`** — можно разместить под строкой имени.
3. Привяжите стиль шрифта (меньше основного имени, цвет золота/серебра).

### Настройка данных (тайтлы для удалённых игроков)
Текущая архитектура сервера НЕ рассылает тайтл другим игрокам зоны при спавне.  
**Для полной реализации:**
- Добавьте поле `equippedTitleDisplayName` в протокол `INIT_PLAYER` / `PLAYER_APPEARED` на чанк-сервере.
- В `UMyGameInstance::SpawnPlayerForClient` после `InitialiseNameplate` вызовите:
  ```cpp
  NewPlayer->SetEquippedTitle(PlayerData.equippedTitleDisplayName);
  ```
- При смене тайтла другим игроком сервер должен рассылать зональный пакет `playerTitleChanged`.

---

## Фичи 3–5 — Звуки скиллов (кулдаун, готовность, нет маны)

### Что было сделано
В структуру `FSkillDefinitionData` (DataStructs.h) добавлены три поля:
- `skillOnCooldownSound` — звук при нажатии скилла на кулдауне.
- `skillReadySound` — звук когда скилл стал доступен после кулдауна.
- `notEnoughManaSound` — звук при нажатии скилла при нехватке маны.

Все три — `TSoftObjectPtr<USoundBase>`, категория `"Skill Definition|UI Feedback"`.

Приоритет: **per-skill звук → глобальный AudioManager UISound (fallback)**.

### Настройка в DataTable
1. Откройте `DT_SkillDefinitions` (или ваш аналог DataTable для скиллов).
2. Найдите нужный скилл.
3. В разделе **Skill Definition | UI Feedback** заполните:
   - **Skill On Cooldown Sound** → звуковой ассет (например, `SFX_Skill_Cooldown`).
   - **Skill Ready Sound** → звуковой ассет (например, `SFX_Skill_Ready`).
   - **Not Enough Mana Sound** → звуковой ассет (например, `SFX_No_Mana`).
4. Если поля пустые — используются глобальные звуки из `AudioManager` (UISound events).

### Настройка глобальных fallback-звуков
В Blueprint `BP_AudioManager` (или DataAsset):
- `EUISoundEvent::SkillCooldownStart` → глобальный "скилл на кулдауне".
- `EUISoundEvent::SkillReady` → глобальный "скилл готов".
- `EUISoundEvent::SkillNotEnoughMana` → глобальный "не хватает маны".

---

## Фича 6 — VFX на харвестабельных трупах

### Что было сделано
В `ABasicMOB` добавлены:
- `UPROPERTY HarvestableVFX` (TSoftObjectPtr<UNiagaraSystem>) — Niagara-система для трупа.
- `UNiagaraComponent* HarvestableVFXComponent` — компонент, который создаётся при смерти и деактивируется при хаврестнутом трупе.
- Метод `RefreshHarvestableVFX()` вызывается при `SetMOBIsDead` и `SetHarvested`.

### Логика
| Состояние моба | VFX |
|---|---|
| Жив | Не создаётся |
| Мёртв, не харвестнут | VFX активен (петля) |
| Мёртв + харвестнут | VFX деактивируется |

### Настройка в Blueprint моба
1. Откройте Blueprint вашего моба (`BP_MOB_*` или `BP_BasicMOB`).
2. В разделе **Harvest | VFX** установите поле **Harvestable VFX** → ваш Niagara-ассет.
3. Рекомендуется использовать **зацикленный** Niagara-эффект (например, мерцание, частицы над трупом).
4. Если поле пустое — VFX не спавнится, поведение без изменений.

> Создайте ассет `NS_HarvestableGlow` или аналог — петля частиц над трупом (цвет: зеленоватый/белый).

---

## Фича 7 — Плавающие числа для HP/MP регенерации

### Что было сделано
- Добавлен тип `EDamageType::ManaRegen` для будущей поддержки серверных тиков регенерации маны.
- `DamageTextWidget::Init()` теперь показывает `+N` с зелёным цветом для `Heal` и сине-голубым для `ManaRegen`.
- HoT-тики (effectTypeSlug == `"hot"`) отображаются как зелёные `+N` числа над персонажем.

### Автоматика
Ничего настраивать не нужно. Числа появляются автоматически при:
- **Мобы:** когда сервер присылает `effectTick` с `effectTypeSlug == "hot"`.
- **Игрок:** через `ShowHealingEffect_Implementation` при HoT-тиках.

### Цвета FCT
| Тип | Цвет | Префикс |
|---|---|---|
| Обычный урон | Белый/красный | — |
| Критический урон | Оранжевый/жёлтый | — |
| Хил (Heal) | Яркий зелёный `(0.1, 1.0, 0.3)` | `+` |
| Рег маны (ManaRegen) | Голубой `(0.2, 0.6, 1.0)` | `+` |

> Пассивная регенерация HP/MP через `stats_update` **не** отображается в FCT специально (слишком частые апдейты). Только HoT-тики (бафы, умения).

---

## Фича 8 — Пузырёк чата над головой игрока

### Что было сделано
- Создан новый виджет `UChatBubbleWidget` (`Public/UI/ChatBubbleWidget.h` + `Private/UI/ChatBubbleWidget.cpp`).
- В `ABasicPlayer` добавлен `UWidgetComponent* ChatBubbleComponent` (WorldSpace, над головой).
- Подписка на `UChatManager::OnChatMessageReceived` — при получении сообщения от ЭТОГО персонажа пузырёк показывается.
- Пузырёк автоматически скрывается через `ChatBubbleDisplayDuration` секунд.

### Настройка Blueprint

#### Шаг 1 — Создайте виджет `WBP_ChatBubble`
1. Создайте User Widget Blueprint: `WBP_ChatBubble` (родитель — `UChatBubbleWidget`).
2. Добавьте `TextBlock` с именем **`MessageText`** (BindWidgetOptional).
3. Настройте фон, шрифт, размер (рекомендуемый: 300×80 юнитов в мировом пространстве).
4. По умолчанию виджет должен быть в состоянии **Collapsed**.

#### Шаг 2 — Назначьте класс виджета в BP_BasicPlayer
1. Откройте `BP_BasicPlayer` (или дочерний Blueprint игрока).
2. В деталях найдите раздел **Chat Bubble**.
3. Установите **Chat Bubble Widget Class** → `WBP_ChatBubble`.
4. Опционально измените **Chat Bubble Display Duration** (по умолчанию 5 секунд).

#### Шаг 3 — Позиция пузырька
`ChatBubbleComponent` создаётся в конструкторе с Z-offset = 220 см (над головой).  
Если нужно подправить — в Blueprint выберите компонент `ChatBubbleComponent` и выставьте нужный Transform.

### Как работает
```
Игрок вводит чат-сообщение
    → UChatManager::OnChatMessageReceived.Broadcast(Message)
        → ABasicPlayer::HandleChatMessageForBubble (фильтр по senderId)
            → ChatBubbleComponent->SetVisibility(true)
            → UChatBubbleWidget::ShowMessage(Message)
                → MessageText = Message.content
                → SetVisibility(HitTestInvisible)
                → таймер HideTimer на DisplayDuration секунд
                    → HideBubble() → Collapsed
```

### Замечания
- Пузырёк НЕ показывается для чужих сообщений — у каждого игрока свой `ABasicPlayer` и своя проверка `senderId`.
- Серверные сообщения с `bIsError == true` игнорируются.
- Ширина виджета в пространстве: 300 юнитов (настраивается через `DrawSize` у `ChatBubbleComponent`).
