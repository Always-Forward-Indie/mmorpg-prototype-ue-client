# Login Flow & Character Visual System — Полное руководство по настройке

Охватывает всё: от нуля до работающего экрана логина с 3D-превью персонажей.

**Что реализовано в коде:**
- Экран логина / регистрации / выбора / создания / удаления персонажа (`ULoginFlowWidget`)
- Сетевой слой через `AuthenticationManager` (TCP JSON, login-server)
- Client-side валидация полей ввода (`LoginFlowValidation`)
- DataTable-маппинг серверных ошибок на понятные сообщения
- 3D-превью персонажей на подиуме (`UCharacterPreviewManager`)
- Визуальная система — меш/анимация/масштаб по DataTable (`FCharacterVisualDefinition`)
- Применение визуала как в логин-экране, так и при спауне в игровом мире

---

## Содержание

1. [Общая архитектура](#1-общая-архитектура)
2. [Настройка BP_GameInstance](#2-настройка-bp_gameinstance)
3. [Настройка Login Level](#3-настройка-login-level)
4. [DataTable ошибок (DT_LoginErrors)](#4-datatable-ошибок-dt_loginerrors)
5. [DataTable визуалов (DT_CharacterVisuals)](#5-datatable-визуалов-dt_charactervisuals)
6. [WBP_CharacterListItem](#6-wbp_characterlistitem)
7. [WBP_LoginFlowWidget](#7-wbp_loginflowwidget)
8. [Позиции подиума и камер](#8-позиции-подиума-и-камер)
9. [Поток данных — как всё работает](#9-поток-данных--как-всё-работает)
10. [Добавление нового класса / расы / пола](#10-добавление-нового-класса--расы--пола)

---

## 1. Общая архитектура

```
Login Level загружается
    └─ MyGameInstance::OnLoginLevelLoaded()
           ├─ Создаёт UCharacterPreviewManager
           ├─ Создаёт WBP_LoginFlowWidget и добавляет на viewport
           └─ Камера по умолчанию = "фоновый вид" (логин/регистрация)

Пользователь логинится / регистрируется
    └─ AuthenticationManager → TCP → login-server
           ├─ OnLoginResponse / OnRegisterResponse
           ├─ Автоматически запрашивает getCharactersList
           ├─ Автоматически запрашивает getCharacterCreationOptions
           └─ OnCharacterListReceived → UI переходит на CharacterSelect
                  └─ CharacterPreviewManager спавнит превью на подиуме

Игрок выбирает "Play"
    └─ MyGameInstance::JoinSelectedCharacterToGame()
           └─ TransitionToGameWorld() → chunk-server → SpawnPlayerForClient()
                  └─ BasicPlayer::ApplyVisualFromDataTable() ← меш + анимация
```

---

## 2. Настройка BP_GameInstance

Открой `BP_GameInstance` (наследник `UMyGameInstance`) → вкладка **Class Defaults**.

### Секция "UI"

| Свойство | Тип | Что указывать |
|---|---|---|
| **Login Flow Widget Class** | `TSubclassOf<ULoginFlowWidget>` | `WBP_LoginFlowWidget` (твой Blueprint-виджет) |
| **Login Error Messages Table** | `UDataTable*` | `DT_LoginErrors` (см. раздел 4) |
| **Characters List Item Widget Class** | `TSubclassOf<UCharacterListItem>` | `WBP_CharacterListItem` (см. раздел 6) |

> `LoginFlowWidgetClass` имеет приоритет над устаревшим `LoginScreenWidgetClass`. Если оба заданы — используется `LoginFlowWidgetClass`.

### Секция "Character Visuals"

| Свойство | Тип | Что указывать |
|---|---|---|
| **Character Visual Definitions** | `UDataTable*` | `DT_CharacterVisuals` (см. раздел 5) |

### Секция "Character Visuals | Podium"

| Свойство | Тип | Описание |
|---|---|---|
| **Podium Spawn Locations** | `TArray<FVector>` | Массив из 1–4 позиций на подиуме |
| **Podium Spawn Rotation** | `FRotator` | Поворот персонажей. По умолчанию `(0, 180, 0)` — лицом к камере |
| **Podium Camera Location** | `FVector` | Позиция камеры для вида "выбор персонажа" |
| **Podium Camera Rotation** | `FRotator` | Поворот камеры для вида "выбор персонажа" |

### Секция "Character Visuals | Create Preview"

| Свойство | Тип | Описание |
|---|---|---|
| **Create Preview Location** | `FVector` | Позиция одиночного персонажа при создании |
| **Create Preview Rotation** | `FRotator` | Поворот персонажа при создании. По умолчанию `(0, 180, 0)` |
| **Create Preview Camera Location** | `FVector` | Позиция камеры — крупный план при создании |
| **Create Preview Camera Rotation** | `FRotator` | Поворот камеры — крупный план при создании |

---

## 3. Настройка Login Level

### Обязательный актор на уровне

На Login Level **должен** присутствовать актор типа `AMyCameraActor`.  
`MyGameInstance::OnLoginLevelLoaded()` находит его автоматически по типу — он становится активной камерой, и `CharacterPreviewManager` перемещает его при переходах между видами.

> Никаких специальных тегов не нужно — достаточно одного `AMyCameraActor` на уровне.

### Начальная позиция камеры

Поставь `AMyCameraActor` в позицию **фонового вида** — то, что игрок видит на экране логина и регистрации (пейзаж, таверна, лагерь). Позиции для вида подиума и крупного плана задаются в `BP_GameInstance` и применяются при переключении панелей.

### Освещение подиума *(рекомендуется)*

Добавь Point/Spot lights в зоне подиума для красивой подсветки превью-персонажей.

---

## 4. DataTable ошибок (DT_LoginErrors)

Таблица маппит серверные `ERR_*` коды на локализованные сообщения.

### Создание

1. **Content Browser** → **Add** → **Miscellaneous** → **Data Table**
2. Структура строки: **`FLoginErrorTableRow`**
3. Сохрани как `DT_LoginErrors` (например, `Content/Data/UI/DT_LoginErrors`)
4. Назначь в `BP_GameInstance` → **Login Error Messages Table**

### Формат

**Row Name** = точный серверный код ошибки (чувствителен к регистру).  
**DisplayMessage** = текст для игрока (поддерживает локализацию).

### Все серверные коды

**Регистрация:**

| Row Name | Рекомендуемый текст |
|---|---|
| `ERR_LOGIN_INVALID` | Invalid login format. Use 3–20 characters: letters, numbers, underscore |
| `ERR_LOGIN_TAKEN` | This login is already taken |
| `ERR_PASSWORD_TOO_SHORT` | Password must be at least 8 characters |
| `ERR_PASSWORD_TOO_LONG` | Password must be 100 characters or less |
| `ERR_EMAIL_INVALID` | Invalid email address |
| `ERR_REGISTER_FAILED` | Registration failed. Please try again |

**Создание персонажа:**

| Row Name | Рекомендуемый текст |
|---|---|
| `ERR_CHAR_NAME_TAKEN` | This character name is already taken |
| `ERR_CHAR_NAME_INVALID` | Invalid name. Use 2–20 characters: letters and spaces |
| `ERR_CHAR_SLOT_FULL` | Maximum 4 characters per account |
| `ERR_CHAR_MISSING_FIELD` | Please fill in all fields |
| `ERR_CHAR_CREATE_FAILED` | Character creation failed. Please try again |

**Удаление персонажа:**

| Row Name | Рекомендуемый текст |
|---|---|
| `ERR_CHARACTER_NOT_FOUND` | Character not found |
| `ERR_INVALID_CHARACTER_ID` | Invalid character |

> Если код не найден в таблице и начинается с `ERR_` — показывается общее "An error occurred. Please try again."

---

## 5. DataTable визуалов (DT_CharacterVisuals)

Таблица задаёт меш, AnimBP, масштаб для каждой комбинации класс + раса + пол. Используется и на логин-экране (превью), и при спауне игрока в игровом мире.

### Создание

1. **Content Browser** → **Add** → **Miscellaneous** → **Data Table**
2. Структура строки: **`FCharacterVisualDefinition`**
3. Сохрани как `DT_CharacterVisuals` (например, `Content/Data/Characters/DT_CharacterVisuals`)
4. Назначь в `BP_GameInstance` → **Character Visual Definitions**

### Формат Row Name

```
classSlug_raceSlug_genderName
```

Примеры: `warrior_human_male`, `mage_elf_female`, `archer_orc_male`

> Слаги должны **точно совпадать** с тем, что сервер возвращает в полях `characterClass`, `characterRace`, `characterGender` — в нижнем регистре.

### Поля строки

| Поле | Тип | Описание |
|---|---|---|
| `ClassSlug` | FString | Slug класса (напр. `warrior`) |
| `RaceSlug` | FString | Slug расы (напр. `human`) |
| `GenderName` | FString | Имя пола (напр. `male`) |
| **Visual.SkeletalMesh** | `TSoftObjectPtr<USkeletalMesh>` | Скелетный меш |
| **Visual.AnimBPClass** | `TSoftClassPtr<UAnimInstance>` | Animation Blueprint класс |
| **Visual.ActorScale** | `FVector` | Масштаб актора (по умолчанию `1, 1, 1`) |
| **Visual.CombatHitHeight** | `float` | Высота hit-box капсулы |
| **Visual.PortraitIcon** | `TSoftObjectPtr<UTexture2D>` | Иконка портрета для UI |
| **Visual.AudioProfileId** | `FName` | ID звукового профиля |
| **Visual.DeathVFX** | `TSoftObjectPtr<UNiagaraSystem>` | VFX при смерти (необязательно) |

### Система Fallback

Если точное совпадение не найдено, код ищет по цепочке:
1. Точное совпадение: `warrior_human_male`
2. Пол по умолчанию (`male`): `warrior_human_male`
3. Раса + пол по умолчанию (`human_male`): `warrior_human_male`

**Достаточно одной строки на класс** для минимально работающего набора.

### Примеры строк

| Row Name | SkeletalMesh | AnimBPClass | ActorScale |
|---|---|---|---|
| `warrior_human_male` | SK_Warrior_Male | ABP_Warrior | 1, 1, 1 |
| `warrior_human_female` | SK_Warrior_Female | ABP_Warrior | 1, 1, 1 |
| `mage_elf_female` | SK_Mage_Female | ABP_Mage | 0.9, 0.9, 0.9 |
| `archer_orc_male` | SK_Archer_Orc | ABP_Archer | 1.05, 1.05, 1.05 |

---

## 6. WBP_CharacterListItem

Строка списка персонажей в панели Character Select.

### Создание

1. **Content Browser** → **Add** → **Blueprint Class**
2. Родительский класс: `UCharacterListItem`
3. Назови `WBP_CharacterListItem`
4. Назначь в `BP_GameInstance` → **Characters List Item Widget Class**

### Обязательный BindWidget

| Имя виджета | Тип | Описание |
|---|---|---|
| `CharacterNameTextBlock` | TextBlock | Отображает строку формата `"Name — Class Lv.X"` |

---

## 7. WBP_LoginFlowWidget

Главный виджет логин-экрана. Содержит `UWidgetSwitcher` с четырьмя панелями.

### Создание

1. **Content Browser** → **Add** → **Blueprint Class**
2. Родительский класс: **`ULoginFlowWidget`**
3. Назови `WBP_LoginFlowWidget`
4. Назначь в `BP_GameInstance` → **Login Flow Widget Class**

### Defaults виджета

| Свойство | Что указывать |
|---|---|
| **Error Messages Table** | `DT_LoginErrors` |

### Корневой виджет: PanelSwitcher

Создай `UWidgetSwitcher` с именем `PanelSwitcher`. Должен содержать ровно **4 дочерних панели в строгом порядке**:

| Индекс | Панель |
|---|---|
| **0** | Login |
| **1** | Registration |
| **2** | Character Select |
| **3** | Character Create |

---

### Панель 0 — Login

| Имя (BindWidget) | Тип | Обязательно |
|---|---|---|
| `Login_UsernameInput` | EditableTextBox | ✅ |
| `Login_PasswordInput` | EditableTextBox | ✅ |
| `Login_LoginButton` | Button | ✅ |
| `Login_RegisterButton` | Button | ✅ |
| `Login_ErrorText` | TextBlock | ✅ |
| `Login_Throbber` | Throbber | ⬜ опционально |

- `Login_ErrorText` скрыт по умолчанию
- Во время запроса: кнопки отключаются, `Login_Throbber` появляется
- Успешный вход → автопереход на Character Select

---

### Панель 1 — Registration

| Имя (BindWidget) | Тип | Обязательно |
|---|---|---|
| `Register_UsernameInput` | EditableTextBox | ✅ |
| `Register_PasswordInput` | EditableTextBox | ✅ |
| `Register_EmailInput` | EditableTextBox | ✅ |
| `Register_CreateAccountButton` | Button | ✅ |
| `Register_BackButton` | Button | ✅ |
| `Register_ErrorText` | TextBlock | ✅ |
| `Register_Throbber` | Throbber | ⬜ опционально |

- `Register_BackButton` → возврат на Login
- Успешная регистрация → автопереход на Character Select

---

### Панель 2 — Character Select

| Имя (BindWidget) | Тип | Обязательно |
|---|---|---|
| `CharacterSelectListView` | ListView | ✅ |
| `CharSelect_PlayButton` | Button | ✅ |
| `CharSelect_CreateNewButton` | Button | ✅ |
| `CharSelect_DeleteButton` | Button | ✅ |
| `CharSelect_LogoutButton` | Button | ✅ |
| `CharSelect_ErrorText` | TextBlock | ✅ |
| `CharSelect_EmptyText` | TextBlock | ⬜ "нет персонажей" |
| `CharSelect_Throbber` | Throbber | ⬜ опционально |
| `CharSelect_DeleteConfirmContainer` | любой Widget | ⬜ контейнер подтверждения |
| `CharSelect_DeleteConfirmPrompt` | TextBlock | ⬜ "введите имя для подтверждения" |
| `CharSelect_DeleteConfirmInput` | EditableTextBox | ⬜ ввод имени |
| `CharSelect_DeleteConfirmButton` | Button | ⬜ "Удалить" |
| `CharSelect_DeleteCancelButton` | Button | ⬜ "Отмена" |

- `CharSelect_PlayButton` / `CharSelect_DeleteButton` активны только при выбранном персонаже
- `CharSelect_CreateNewButton` активна пока персонажей < 4
- Кнопка подтверждения удаления включается только когда введённое имя совпадает

> **ListView → Entry Widget Class:** открой `CharacterSelectListView` в дизайнере → Details → **Entry Widget Class** = `WBP_CharacterListItem`.

---

### Панель 3 — Character Create

| Имя (BindWidget) | Тип | Обязательно |
|---|---|---|
| `CharCreate_NameInput` | EditableTextBox | ✅ |
| `CharCreate_ClassComboBox` | ComboBoxString | ✅ |
| `CharCreate_RaceComboBox` | ComboBoxString | ✅ |
| `CharCreate_GenderComboBox` | ComboBoxString | ✅ |
| `CharCreate_ClassDescription` | TextBlock | ✅ |
| `CharCreate_CreateButton` | Button | ✅ |
| `CharCreate_BackButton` | Button | ✅ |
| `CharCreate_ErrorText` | TextBlock | ✅ |
| `CharCreate_Throbber` | Throbber | ⬜ опционально |

- Опции ComboBox заполняются автоматически из ответа сервера `getCharacterCreationOptions`
- `CharCreate_ClassDescription` обновляется при смене класса
- Смена класса/расы/пола → мгновенный апдейт 3D-превью на подиуме

---

## 8. Позиции подиума и камер

### Как выставить позиции

1. Открой Login Level
2. Вытащи из **Place Mode** пустые `Actor`-ы — расставь их на сцене там, где должны стоять персонажи
3. Для каждого скопируй **Details → Transform → Location**
4. Вставь в `BP_GameInstance` → `PodiumSpawnLocations[0..3]`
5. Удали временные акторы
6. Повтори для позиций камер (`PodiumCameraLocation`, `CreatePreviewCameraLocation`)

### Рекомендации

| Точка | Совет |
|---|---|
| **Podium [0..3]** | Расставь в ряд или полукругом, шаг ~100–120 cm |
| **Podium Camera** | Смотри на центр подиума, угол ~15–25° вниз |
| **Create Preview** | По центру подиума или чуть ближе к камере |
| **Create Preview Camera** | Ближе, чем podium camera — крупный план по пояс или во весь рост |

### Поворот персонажей

По умолчанию `PodiumSpawnRotation = (0, 180, 0)` — персонажи развёрнуты лицом к камере (Yaw 180°). Если камера смотрит с другой стороны — подбери нужный Yaw.

---

## 9. Поток данных — как всё работает

### Логин / Регистрация

```
[Игрок нажимает Login]
  LoginFlowWidget → AuthenticationManager::SendLoginRequest()
    → TCP → login-server
    ← authentificationClient response (success)
  AuthManager автоматически:
    → SendCharacterListRequest()
    → SendCharacterCreationOptionsRequest()
    ← getCharactersList      → OnCharacterListReceived
    ← getCharacterCreationOptions → OnCreationOptionsReceived (кэшируется)

[OnCharacterListReceived]
  PopulateCharacterList()                            ← заполнить ListView
  SwitchToPanel(CharacterSelect)
  CharacterPreviewManager::SpawnCharacterPreviews()  ← 3D акторы на подиуме
  CharacterPreviewManager::BlendToSelectCamera()     ← плавный переезд камеры
```

### Создание персонажа

```
[Нажать "Create New"]
  CharacterPreviewManager::ClearSelectPreviews()
  PopulateCreationOptions()                          ← заполнить комбобоксы
  UpdateCreatePreviewFromCombos()                    ← спавн одного превью
  CharacterPreviewManager::BlendToCreateCamera()     ← крупный план
  SwitchToPanel(CharacterCreate)

[Смена класса / расы / пола в комбобоксах]
  CharacterPreviewManager::UpdateCreatePreview(class, race, gender)  ← live update

[Нажать "Back"]
  CharacterPreviewManager::ClearCreatePreview()
  CharacterPreviewManager::SpawnCharacterPreviews(CachedCharacters)
  CharacterPreviewManager::BlendToSelectCamera()
  SwitchToPanel(CharacterSelect)

[Нажать "Create" — успех]
  AuthManager автоматически re-запрашивает список
  → OnCharacterListReceived → обновить список + превью
```

### Удаление персонажа

```
[Нажать "Delete"]
  ShowDeleteConfirmation()        ← inline UI с полем ввода имени

[Ввести имя + нажать "Confirm"]
  AuthManager::SendDeleteCharacterRequest(CharacterId)
  ← deleteCharacter (success)
  → Убрать из CachedCharacters
  → PopulateCharacterList()       ← обновить список и превью
```

### Вход в игру

```
[Нажать "Play"]
  MyGameInstance::SetCurrentCharacterID(CharId)
  MyGameInstance::JoinSelectedCharacterToGame()
    → CharacterPreviewManager::Cleanup()   ← уничтожить все превью-акторы
    → TransitionToGameWorld()
    → chunk-server → SpawnPlayerForClient()
         NewPlayer->SetPlayerClass(...)
         NewPlayer->SetPlayerRace(...)
         NewPlayer->SetPlayerGender(...)
         NewPlayer->ApplyVisualFromDataTable(CharacterVisualDefinitionsTable)
              ← async загрузка SkeletalMesh + AnimBP
              ← автоподгонка капсулы под размер меша
              ← применение ActorScale из таблицы
```

### Logout / Сессия истекла

```
  CharacterPreviewManager::ClearSelectPreviews()
  CharacterPreviewManager::ClearCreatePreview()
  CharacterPreviewManager::BlendToLoginCamera()   ← камера возвращается на фон
  SwitchToPanel(Login)
```

---

## 10. Добавление нового класса / расы / пола

1. Убедись, что сервер возвращает новые опции в `getCharacterCreationOptions`
2. Добавь строки в `DT_CharacterVisuals` (слаги — как у сервера, в нижнем регистре)
3. Больше ничего в коде менять не нужно

### Минимальный набор строк для нового класса

Если разных мешей по расам/полу нет — достаточно **одной строки**:

```
Row Name:          newclass_human_male
ClassSlug:         newclass
RaceSlug:          human
GenderName:        male
Visual.SkeletalMesh: SK_NewClass
Visual.AnimBPClass:  ABP_NewClass
Visual.ActorScale:   (1, 1, 1)
```

Fallback-цепочка автоматически будет использовать эту строку для всех рас и для пола female.
