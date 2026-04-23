# Login Flow & Character Select/Create — План реализации

## Текущее состояние

### Сервер (login-server) — всё готово
| Эндпоинт | eventType | Статус |
|---|---|---|
| Регистрация | `registerAccount` | ✅ Реализовано |
| Авторизация | `authentificationClient` | ✅ Реализовано |
| Опции создания персонажа | `getCharacterCreationOptions` | ✅ Реализовано |
| Список персонажей | `getCharactersList` | ✅ Реализовано |
| Создание персонажа | `createCharacter` | ✅ Реализовано |
| Удаление персонажа | `deleteCharacter` | ✅ Реализовано |

### Клиент (UE5) — частично
| Функция | Статус | Что есть |
|---|---|---|
| Login (`authentificationClient`) | ✅ | `SendLoginRequest` в `AuthenticationManager` |
| Character list (`getCharactersList`) | ✅ | `SendCharacterListRequest` + отображение в `LoginWidget` |
| Registration (`registerAccount`) | ❌ | Не реализовано |
| Character creation options (`getCharacterCreationOptions`) | ❌ | Не реализовано |
| Create character (`createCharacter`) | ❌ | Не реализовано |
| Delete character (`deleteCharacter`) | ❌ | Не реализовано |

### Известные проблемы
- Валидация пароля на клиенте (3–20 символов) расходится с сервером (8–100 символов)
- Нет маппинга серверных `ERR_*` кодов в человекочитаемые сообщения
- `LoginWidget` — отладочная заглушка с хардкоженными TestUser1/2/3
- Один монолитный виджет пытается быть и экраном логина, и экраном выбора персонажа

---

## Архитектура: LoginLevel

Всё происходит в одном LoginLevel. Два положения камеры, переключение через `SetViewTargetWithBlend`.

### Позиции камеры

**Позиция 1 — Фоновая сцена (Login/Register)**
- Камера смотрит на окружение (пейзаж, таверна, лагерь — по дизайну уровня)
- Используется текущая `AMyCameraActor` с `LoginLevelCameraLocation` / `LoginLevelCameraRotation`
- Показывается поверх: LoginPanel или RegistrationPanel

**Позиция 2 — Подиум персонажей (Character Select/Create)**
- Камера смотрит на площадку, где стоят персонажи (`ABasicPlayer`)
- Вторая `AMyCameraActor` (или просто вторая позиция/Target Point в уровне)
- Управляемое освещение: point/spot lights для красивой подсветки персонажей
- До 4 точек спавна персонажей (по максимуму слотов аккаунта)
- Показывается поверх: CharacterSelectPanel или CharacterCreatePanel

### Переходы между экранами

| Откуда | Куда | Камера | UI |
|---|---|---|---|
| Запуск игры | Login | Позиция 1 (фон) | LoginPanel |
| Login | Register | Позиция 1 (без изменений) | WidgetSwitcher → RegistrationPanel |
| Register → успех | Character Select | Blend к Позиции 2 | WidgetSwitcher → CharSelectPanel |
| Login → успех | Character Select | Blend к Позиции 2 | WidgetSwitcher → CharSelectPanel |
| Character Select | Character Create | Камера blend ближе к одному слоту | WidgetSwitcher → CharCreatePanel |
| Character Create → успех | Character Select | Камера возвращается к Позиции 2 | WidgetSwitcher → CharSelectPanel |
| Character Create → отмена | Character Select | Камера возвращается к Позиции 2 | WidgetSwitcher → CharSelectPanel |
| Character Select → Play | TransitionToGameWorld | Загрузка игрового мира |
| Character Select → Logout | Позиция 1 | WidgetSwitcher → LoginPanel |

---

## Phase 1 — UI Architecture (WidgetSwitcher)

Заменить текущий `LoginWidget` на новый `LoginFlowWidget` с `UWidgetSwitcher` и четырьмя child-панелями.

### LoginFlowWidget (корневой виджет)
- Содержит `UWidgetSwitcher`
- Управляет переключением панелей
- Хранит общий контекст (clientId, hash) — доступен всем панелям без прокидывания

### Панели

**LoginPanel**
- Поле ввода Login
- Поле ввода Password
- Кнопка "Login"
- Кнопка "Register" (переключает на RegistrationPanel)
- Текст ошибки (скрыт по умолчанию)
- Опционально: "Remember login" checkbox (сохраняет только login, НЕ пароль)

**RegistrationPanel**
- Поле ввода Login
- Поле ввода Password
- Поле ввода Email (опционально)
- Кнопка "Create Account"
- Кнопка "Back" (возврат на LoginPanel)
- Текст ошибки

**CharacterSelectPanel**
- Список персонажей (до 4), каждый элемент: имя, класс, уровень
- Кнопка "Play" (активна при выбранном персонаже)
- Кнопка "Create New" (активна если слотов < 4)
- Кнопка "Delete" (с confirmation dialog)
- Кнопка "Logout" (возврат на LoginPanel, очистка clientId/hash)
- При пустом списке — сообщение "No characters" + автофокус на "Create New"

**CharacterCreatePanel**
- Поле ввода имени персонажа
- Выбор класса (ComboBox) — данные из `getCharacterCreationOptions`
- Выбор расы (ComboBox) — данные из `getCharacterCreationOptions`
- Выбор пола (ComboBox) — данные из `getCharacterCreationOptions`
- Описание выбранного класса (текстовое поле, обновляется при смене)
- Кнопка "Create"
- Кнопка "Back" (возврат на CharacterSelectPanel)
- Текст ошибки

---

## Phase 2 — Network Layer (AuthenticationManager)

### Новые методы отправки запросов

| Метод | eventType | Когда вызывается |
|---|---|---|
| `SendRegisterRequest(Login, Password, Email)` | `registerAccount` | Из RegistrationPanel |
| `SendCharacterCreationOptionsRequest()` | `getCharacterCreationOptions` | Автоматически после логина/регистрации |
| `SendCreateCharacterRequest(Name, Class, Race, Gender)` | `createCharacter` | Из CharacterCreatePanel |
| `SendDeleteCharacterRequest(CharacterId)` | `deleteCharacter` | Из CharacterSelectPanel |

### Обработка ответов

Расширить `ProcessLoginResponse` (или переименовать в `ProcessServerResponse`) для обработки новых eventType:

- `registerAccount` + `success` → сохранить clientId/hash, запросить character list + creation options, перейти на CharacterSelectPanel
- `registerAccount` + `error` → показать ошибку на RegistrationPanel
- `getCharacterCreationOptions` + `success` → сохранить списки классов/рас/полов для CharacterCreatePanel
- `createCharacter` + `success` → обновить список персонажей (повторный `getCharactersList`), перейти на CharacterSelectPanel
- `createCharacter` + `error` → показать ошибку на CharacterCreatePanel
- `deleteCharacter` + `success` → убрать персонажа из списка, деспавнить preview-актора
- `deleteCharacter` + `error` → показать ошибку
- Любой `Unauthorized` → очистить clientId/hash, вернуться на LoginPanel

### Новые делегаты (для связи AuthenticationManager → UI)

| Делегат | Параметры | Назначение |
|---|---|---|
| `OnRegisterResponse` | status, message | Результат регистрации |
| `OnCharacterCreationOptionsReceived` | classes[], races[], genders[] | Данные для CharacterCreatePanel |
| `OnCreateCharacterResponse` | status, message, characterId | Результат создания |
| `OnDeleteCharacterResponse` | status, message, characterId | Результат удаления |

---

## Phase 3 — Client-side валидация

Синхронизировать клиентскую валидацию с серверными ограничениями. Валидация на клиенте — только для UX (мгновенный фидбек). Сервер всегда авторитетен.

| Поле | Клиент проверяет | Сервер проверяет |
|---|---|---|
| login | 3–20 символов, `[A-Za-z0-9_]` | то же |
| password | **8–100 символов** (исправить текущие 3–20!) | 8–100 |
| email | содержит `@` если не пустой | то же |
| characterName | 2–20 символов, буквы и пробелы | то же |
| characterClass | не пустой, есть в списке от сервера | то же |
| characterRace | не пустой, есть в списке от сервера | то же |
| characterGender | не пустой, есть в списке от сервера | то же |

---

## Phase 4 — Error Handling (маппинг ERR_* → UI)

Создать TMap маппинг серверных кодов ошибок в локализуемые строки (NSLOCTEXT).

### Коды для маппинга

**Регистрация:**
- `ERR_LOGIN_INVALID` → "Invalid login format. Use 3–20 characters: letters, numbers, underscore"
- `ERR_LOGIN_TAKEN` → "This login is already taken"
- `ERR_PASSWORD_TOO_SHORT` → "Password must be at least 8 characters"
- `ERR_PASSWORD_TOO_LONG` → "Password must be 100 characters or less"
- `ERR_EMAIL_INVALID` → "Invalid email address"
- `ERR_REGISTER_FAILED` → "Registration failed. Please try again"

**Создание персонажа:**
- `ERR_CHAR_NAME_TAKEN` → "This character name is already taken"
- `ERR_CHAR_NAME_INVALID` → "Invalid name. Use 2–20 characters: letters and spaces"
- `ERR_CHAR_SLOT_FULL` → "Maximum 4 characters per account"
- `ERR_CHAR_MISSING_FIELD` → "Please fill in all fields"
- `ERR_CHAR_CREATE_FAILED` → "Character creation failed. Please try again"

**Удаление персонажа:**
- `ERR_CHARACTER_NOT_FOUND` → "Character not found"
- `ERR_INVALID_CHARACTER_ID` → "Invalid character"

**Общие:**
- `Unauthorized` → автоматический возврат на LoginPanel с сообщением "Session expired. Please log in again"

---

## Phase 5 — 3D Preview персонажей в LoginLevel

### Экран выбора персонажа (Character Select)

- При получении `getCharactersList` — спавним `ABasicPlayer` на заранее заданных точках подиума (до 4)
- Каждый персонаж отображается с дефолтной моделью своего класса (Mage/Warrior/etc)
- Idle-анимация
- При клике на персонажа в UI — подсветка / камера blend к нему
- При выборе "Play" — `TransitionToGameWorld()`

### Экран создания персонажа (Character Create)

- На подиуме один `ABasicPlayer` — preview создаваемого персонажа
- При смене класса → свап модели/AnimBlueprint на preview-акторе
- При смене расы → свап модели (если разные расы имеют разные меши)
- При смене пола → свап модели
- Камера blend ближе к этому одному персонажу

### Что нужно в LoginLevel (Blueprint/Level Design)

- 4 × Target Point для позиций спавна персонажей на подиуме
- 1 × CameraActor (или Target Point) для "позиции 2" камеры (вид на подиум)
- Point/Spot lights для подсветки подиума
- Опционально: фоновый декор за подиумом (стена, баннеры, факелы)

### Data Asset — маппинг Class → Visual

Нужен Data Asset или DataTable для связи:

| Class slug | SkeletalMesh (male) | SkeletalMesh (female) | AnimBlueprint | Idle Montage |
|---|---|---|---|---|
| `mage` | SM_Mage_M | SM_Mage_F | ABP_Mage | AM_Mage_Idle |
| `warrior` | SM_Warrior_M | SM_Warrior_F | ABP_Warrior | AM_Warrior_Idle |

---

## Phase 6 — UX Polish

### Блокировка UI при ожидании ответа
- Disable кнопки "Login" / "Register" / "Create" на время запроса
- Показывать loading indicator (spinner / throbber)
- Предотвращает двойные нажатия и race conditions

### Confirmation dialog при удалении персонажа
- Popup: "Type the character name to confirm deletion"
- Поле ввода имени + кнопки "Delete" / "Cancel"
- Кнопка "Delete" активна только если введённое имя совпадает

### Remember Login
- Сохранять последний успешный login в `GameUserSettings` или `SaveGame`
- При запуске — автоподставлять в поле Login
- Никогда не сохранять пароль

### Клавиатурная навигация
- Enter на LoginPanel / RegistrationPanel → отправить форму
- Escape на CharacterCreatePanel → назад к CharacterSelect
- Tab — переключение между полями ввода

---

## Порядок реализации (приоритет)

### Приоритет 1 — Функциональный минимум
1. Исправить валидацию пароля (8–100 вместо 3–20) — багфикс
2. `LoginFlowWidget` с WidgetSwitcher (LoginPanel + CharacterSelectPanel) — замена текущего LoginWidget
3. `RegistrationPanel` + `SendRegisterRequest` — возможность регистрации из клиента
4. `CharacterCreatePanel` + `getCharacterCreationOptions` + `SendCreateCharacterRequest` — возможность создания персонажа
5. `SendDeleteCharacterRequest` + confirmation dialog — удаление персонажа

После этого этапа: новый игрок может зарегистрироваться, создать персонажа и войти в игру — полностью из клиента, без ручного вмешательства.

### Приоритет 2 — Error handling и стабильность
6. Маппинг `ERR_*` → локализуемые строки
7. Блокировка UI при ожидании ответа
8. Обработка `Unauthorized` → автовозврат на LoginPanel

### Приоритет 3 — 3D Preview персонажей
9. Позиции спавна и камеры в LoginLevel (Level Design)
10. Спавн `ABasicPlayer` на подиуме при получении character list
11. Data Asset: маппинг class → SkeletalMesh / AnimBP
12. Смена модели на preview при переключении класса/расы/пола

### Приоритет 4 — Polish
13. Remember Login
14. Клавиатурная навигация (Enter/Escape/Tab)
15. Camera blend между позициями
16. Per-class idle анимации на preview
17. Вращение preview-персонажа мышью

---

## Необходимые изменения по файлам (обзор)

### Клиент (UE5)

| Файл/Класс | Изменения |
|---|---|
| `LoginWidget` (.h/.cpp) | Заменяется на `LoginFlowWidget` (или рефакторится в него) |
| `AuthenticationManager` (.h/.cpp) | +4 метода отправки, расширение ProcessLoginResponse, +4 делегата |
| `MyGameInstance` (.h/.cpp) | Замена `LoginScreenWidgetClass`/`LoginScreenWidget` на новый тип, хранение creation options |
| Новый: `LoginFlowWidget` (.h/.cpp) | WidgetSwitcher + 4 панели, логика переключения |
| Новый: `CharacterPreviewManager` (.h/.cpp) | Спавн/деспавн preview-акторов, управление камерой подиума (Phase 5) |
| Новый: Data Asset или DataTable | Маппинг class slug → visual assets (Phase 5) |

### Сервер
Сервер не требует изменений для базовой реализации. Все необходимые эндпоинты уже существуют.

Возможное расширение в будущем: добавить `equippedItems` в ответ `getCharactersList` для отображения экипировки на preview.

### LoginLevel (Blueprint/Level Design)
- Добавить площадку для персонажей
- 4 Target Point для позиций спавна
- CameraActor или Target Point для второй позиции камеры
- Освещение подиума
