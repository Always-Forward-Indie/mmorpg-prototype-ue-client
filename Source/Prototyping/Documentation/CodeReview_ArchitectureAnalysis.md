# Анализ архитектуры клиента MMORPG — Критические узкие места

> **Дата:** 2025  
> **Проект:** mmorpg-prototype-ue-client (UE 5.3/5.4)  
> **Охват:** Глобальный архитектурный разбор, без мелочей стиля кода

---

## Краткое резюме

Проект развивается в правильном направлении — есть разделение менеджеров, интерфейсный подход в боевой системе, TimeSyncService для lag compensation, разделение Worker-потоков для сети. Однако на архитектурном уровне накопилось несколько серьёзных проблем, которые будут тормозить развитие и создавать трудноуловимые баги при масштабировании.

---

## 1. КРИТИЧНО — Сетевой уровень: JSON поверх raw TCP без фреймирования пакетов

### Проблема

Текущая схема передачи данных:
```
[raw bytes TCP] ? разделитель '\n' ? FString ? JSON парсинг
```

`NetworkReceiverWorker::Run()` накапливает байты в `AccumulatedBuffer` и ищет `'\n'`. При каждом входящем пакете выполняется **полный JSON парсинг дважды**: один раз в `AddClientReceiveTimestamp()` (ещё в потоке воркера), и второй раз — в каждом подписанте через `JSONParser::DeserializeMessageData()` и `JSONParser::DeserializeClientData()`. Это означает, что один пакет десериализуется **3-5 раз** в разных местах.

### Последствия
- Избыточная нагрузка на CPU при высоком трафике (MMORPG с 50+ игроками)
- `FString` как транспорт между потоками — постоянные аллокации, нет zero-copy
- `TQueue` (`DataQueue`) без лимита — при лагах сервера или при отладке очередь будет расти бесконечно
- JSON как протокол на горячем пути движения персонажей неприемлем по производительности

### Что нужно сделать
1. **Заменить JSON на бинарный протокол** (FlatBuffers, Protobuf, или хотя бы самописный бинарный формат) для высокочастотных пакетов (`moveCharacter`, обновления позиций MOB)
2. Ввести **фреймирование пакетов** с длиной в заголовке (4 байта `uint32` length-prefix) вместо `'\n'`-delimiter — '\n' легко ломается если в данных когда-нибудь появится base64 или бинарные данные
3. Десериализовывать JSON **один раз** при получении, передавать уже разобранный `TSharedPtr<FJsonObject>` или структуру через очередь
4. Добавить **лимит очереди** (`TQueue` заменить на кольцевой буфер с ограничением)

---

## 2. КРИТИЧНО — `MyGameInstance` — Бог-объект (God Object)

### Проблема

`MyGameInstance` создаёт и владеет **всеми системами** проекта:
```cpp
NetworkManager, PingManager, AuthenticationManager, PlayerManager, MOBManager,
SpawnZoneManager, ItemManager, InventoryManager, HarvestManager, ExperienceManager,
ExperienceNetworkHandler, CombatSystemManager, SkillSystemManager, CombatNetworkHandler,
NPCManager, NPCNetworkHandler, TimeSyncService, PlayerSkillManager, 
SkillDefinitionRepository, PlayerSkillNetworkHandler, PlayerSkillSystemFactory...
```

Это **20+ систем** в одном объекте. При этом `InitNetworkingSetup()` — это метод на ~200 строк с ручной инициализацией каждой системы в строгом порядке зависимостей.

Прямой доступ к менеджерам через `GameInstance` используется везде:
```cpp
// BasicPlayer.cpp
MyGameInstance->PlayerManager->SendMovePlayerRequest(playerData);
MyGameInstance->GetHarvestManager()
MyGameInstance->GetCombatSystemManager()
// и т.д. — прямая связанность с GameInstance в каждом классе
```

### Последствия
- Любое изменение порядка инициализации ломает всё
- Невозможно тестировать системы по отдельности
- При добавлении новой системы нужно трогать `MyGameInstance` — нарушение OCP
- Сильная связанность: `BasicPlayer` знает о `PlayerManager`, который знает о `GameInstance`, который знает о `BasicPlayer`

### Что нужно сделать
1. Ввести **Service Locator** или **Dependency Injection контейнер** — передавать зависимости через конструктор/Initialize, а не брать из GameInstance
2. Выделить `IServiceLocator` интерфейс и использовать его вместо прямого каста к `UMyGameInstance`
3. Разбить `InitNetworkingSetup()` на независимые фабричные методы для каждой подсистемы
4. **Публичный доступ** `MyGameInstance->PlayerManager->...` заменить на методы GameInstance (`MyGameInstance->SendMoveRequest(...)`)

---

## 3. КРИТИЧНО — Трёхкратное дублирование кода Connect/Poll/Worker по серверам

### Проблема

В `NetworkManager` логика подключения, переподключения, создания воркеров и поллинга **написана три раза** — для LoginServer, GameServer и ChunkServer:

```cpp
ConnectLoginServer()  ? ConnectGameServer()  ? ConnectChunkServer()
StartPollingLoginServer() ? StartPollingGameServer() ? StartPollingChunkServer()
PollLoginServerNetworkData() ? PollGameServerNetworkData() ? PollChunkServerNetworkData()
ShowLoginServerConnectionIssuePopup() ? ShowGameServerConnectionIssuePopup() ? ShowChunkServerConnectionIssuePopup()
```

Каждый блок — ~50 строк идентичного кода с разными именами переменных. Итого: ~600 строк вместо ~100.

### Последствия
- Баг-фикс нужно применять в 3 местах — гарантированно что-то забудешь
- Уже есть: `OnGameServerConnectionRetry()` содержит `UE_LOG` дважды (copy-paste баг)
- При добавлении 4-го сервера — снова копировать всё

### Что нужно сделать
Ввести структуру конфигурации сервера и обобщённые методы:
```cpp
struct FServerConnectionConfig
{
    FString IP;
    int32 Port;
    FString Name;
    int32 MaxRetries;
};

// Один метод вместо трёх:
void ConnectToServer(EServerType Type, const FServerConnectionConfig& Config);
void StartPolling(EServerType Type, float Interval);
```

---

## 4. ВЫСОКИЙ — Утечки памяти и опасное управление потоками

### Проблема

В `NetworkManager::Shutdown()` закомментирован код очистки:
```cpp
//delete ReceiverLoginServerWorker;
//ReceiverLoginServerWorker = nullptr;
//delete ReceiverLoginServerThread;
//ReceiverLoginServerThread = nullptr;
```

Аналогично для всех шести воркеров. Деструктор `NetworkSenderWorker` содержит:
```cpp
NetworkSenderWorker::~NetworkSenderWorker()
{
    if (!bRunThread)  // БАГИ: условие логически инвертировано
    {
        Stop();
    }
}
```

Воркеры создаются через `new` (`ReceiverLoginServerWorker = new NetworkReceiverWorker(...)`) — это raw pointers без RAII.

### Последствия
- При переподключении (`ConnectLoginServer()` вызывается повторно через `OnLoginServerConnectionRetry`) — старые воркеры не удаляются, новые создаются ? утечка потоков
- `bRunThread` в воркерах — `bool` без `std::atomic` или `volatile` — гонка данных между main thread и worker thread
- `TimeSyncService` в воркерах — raw pointer на UObject из другого потока, без какой-либо синхронизации

### Что нужно сделать
1. Использовать `TUniquePtr<NetworkReceiverWorker>` вместо raw pointers
2. `bRunThread` ? `std::atomic<bool>` или `TAtomic<bool>`
3. Перед созданием нового воркера при переподключении — гарантированно останавливать и удалять старый
4. Доступ к `UTimeSyncService` из потока воркера — вынести в отдельный thread-safe кэш (например, `TAtomic<int64>` для timestamp вместо вызова метода UObject)

---

## 5. ВЫСОКИЙ — Отсутствие отдельного слоя протокола / Message Dispatcher

### Проблема

`PlayerManager::ProcessChunkServerData()` — это монолитный `if-else` блок на ~150 строк, где каждый `eventType` обрабатывается inline:
```cpp
if (MessageData.eventType == "joinGameCharacter") { ... }
if (MessageData.eventType == "getConnectedCharacters") { ... }
if (MessageData.eventType == "moveCharacter") { ... }
if (MessageData.eventType == "disconnectClient") { ... }
if (MessageData.eventType == "stats_update") { ... }
```

При этом для каждого события выполняется один и тот же JSON парсинг целиком. Аналогично в `AuthenticationManager::ProcessLoginResponse()`.

Нет единой точки регистрации обработчиков — добавление нового типа события требует изменения монолитного if-блока.

### Что нужно сделать
Реализовать **Message Dispatcher** паттерн:
```cpp
// Регистрация один раз при инициализации:
Dispatcher->RegisterHandler("moveCharacter", this, &UPlayerManager::HandleMoveCharacter);
Dispatcher->RegisterHandler("disconnectClient", this, &UPlayerManager::HandleDisconnect);

// ProcessData становится тривиальным:
void UPlayerManager::ProcessChunkServerData(const FString& Data)
{
    FParsedPacket Packet = ParseOnce(Data);
    Dispatcher->Dispatch(Packet);
}
```

---

## 6. ВЫСОКИЙ — Инициализация систем в `BeginPlay` с таймером через `0.5f` секунды

### Проблема

В `ABasicPlayer::BeginPlay()`:
```cpp
FTimerHandle TimerHandle;
GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
{
    if (playerData.isOtherClient) return; // guard
    UIManager->Initialize(InventoryManager, HarvestManager, ExperienceManager, SkillManager);
    // ...
}, 0.5f, false); // "небольшая задержка чтобы всё успело загрузиться"
```

Это **антипаттерн**: задержка в 0.5 секунды — это просто надежда на то, что системы успеют инициализироваться. В зависимости от производительности машины, загрузки уровня и состояния сети это условие может не выполниться или сработать слишком рано.

Аналогично в `BasicMOB::BeginPlay()`:
```cpp
GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ABasicMOB::InitializeUIDelayed, 0.1f, false);
```

### Что нужно сделать
1. Использовать **событийную модель**: `OnManagersReady` делегат в GameInstance
2. Убрать магические задержки и заменить на `PostBeginPlay()` / `OnCharacterDataReceived()` callbacks
3. Добавить `bIsInitialized` флаги в менеджеры с проверкой перед использованием (частично уже есть в NPCManager)

---

## 7. СРЕДНИЙ — `JSONParser::GetTimeSyncService()` использует `GWorld`

### Проблема

```cpp
UTimeSyncService* JSONParser::GetTimeSyncService()
{
    if (GWorld)
    {
        UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GWorld->GetGameInstance());
        // ...
    }
    return nullptr;
}
```

`JSONParser` — это статический класс без контекста. Он обращается к глобальному `GWorld` для получения зависимости. Это:
- Не работает в Unit-тестах
- Потенциально небезопасно в мультипоточном контексте (GWorld может меняться при смене уровней)
- Скрытая зависимость, невидимая из сигнатуры метода

### Что нужно сделать
Передавать `UTimeSyncService*` явно через параметр во все методы `SerializeJson`, либо сделать `JSONParser` обычным `UObject` с инжектированной зависимостью. Метод `SerializeJsonWithTimeSync` уже делает это правильно — нужно убрать `SerializeJson` без параметра или сделать его deprecated.

---

## 8. СРЕДНИЙ — Атака на `GetAllActorsOfClass` в каждом `BeginPlay` / input-событии

### Проблема

```cpp
// BasicPlayer.cpp — OnAttackInput():
TArray<AActor*> FoundActors;
UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABasicMOB::StaticClass(), FoundActors);
```

`GetAllActorsOfClass` итерирует **все акторы на уровне** при каждом нажатии кнопки атаки. В MMORPG с сотнями MOB это O(n) итерация на каждый input event.

### Что нужно сделать
- Использовать `MOBManager`, который уже хранит `TMap<int32, ABasicMOB*>` — запрашивать ближайших через него, или
- Использовать `OverlapMultiByChannel` / `SweepMultiByChannel` в радиусе атаки вместо перебора всех акторов

---

## 9. СРЕДНИЙ — Отсутствие Предикативного (Client-Side Prediction) движения

### Проблема

`UpdateCurrentPlayerMovement()` отправляет позицию на сервер и ждёт ответа. Двигаться игрок может локально, но его авторитетная позиция (`playerData.characterData.characterPosition`) обновляется только при получении `moveCharacter` от сервера. При пинге 100-200мс (типичный MMORPG сервер) движение будет казаться «прилипшим».

`UpdateRemotePlayerMovement()` использует линейную интерполяцию между `LastReceivedPosition` и `TargetReceivedPosition` — это базовое решение, но при переменном пинге даёт заметные рывки.

### Что нужно сделать
1. Внедрить **Client-Side Prediction**: применять локальное движение немедленно, хранить историю входов (input history), при получении серверной позиции — делать reconciliation
2. Для удалённых игроков рассмотреть **Entity Interpolation** с буфером 2-3 серверных тика
3. `ServerPositionUpdateInterval` — вынести в конфиг или синхронизировать с реальным server tick rate

---

## 10. СРЕДНИЙ — Дублирование `InventoryManager` в GameInstance и BasicPlayer

### Проблема

`UInventoryManager` создаётся дважды:
- В `MyGameInstance::Init()`: `InventoryManager = NewObject<UInventoryManager>(this)`
- В `ABasicPlayer` конструкторе: `InventoryManager = CreateDefaultSubobject<UInventoryManager>(TEXT("InventoryManager"))`

В `ABasicPlayer::BeginPlay()` InventoryManager игрока затем регистрируется обратно в GameInstance:
```cpp
MyGameInstance->SetInventoryManager(InventoryManager); // Перезаписывает тот, что создан в GameInstance::Init()
```

Тот InventoryManager, что создан в GameInstance::Init() — никогда не используется, но живёт и тратит память. При этом логика Subscribe/Initialize вызывается в нескольких местах.

### Что нужно сделать
Определить **единственный owner** InventoryManager. Для игрока-специфичных систем (Inventory, HarvestManager) — создавать только в контексте игрока, не в GameInstance. GameInstance хранит только ссылку, которую получает от PlayerActor при спауне.

---

## 11. НИЗКИЙ-СРЕДНИЙ — Все менеджеры используют `UE_LOG(LogTemp, Warning, ...)` для рабочего флоу

### Проблема

Весь код использует `LogTemp` с уровнем `Warning` даже для штатных событий ("GameInstance found", "Network Manager found", "Polling timer set up successfully"). При включённом логировании в runtime это:
- Загрязняет Output Log
- Создаёт overhead от форматирования строк даже в Shipping build (если не обёрнуто в `DO_CHECK`)

### Что нужно сделать
1. Создать именованные категории логов: `DECLARE_LOG_CATEGORY_EXTERN(LogNetwork, Log, All)`
2. Штатные события — `UE_LOG(LogNetwork, Verbose, ...)` или `Log`
3. Ошибки — `Error`, предупреждения — `Warning`
4. Обернуть debug-логи в `#if !UE_BUILD_SHIPPING`

---

## Приоритизированный план рефакторинга

| # | Проблема | Приоритет | Сложность | Риск при откладывании |
|---|----------|-----------|-----------|----------------------|
| 1 | Тройное дублирование NetworkManager | ?? Критично | Средняя | Баги в одном из серверов |
| 2 | Утечки потоков при переподключении | ?? Критично | Средняя | Краш в production |
| 3 | `bRunThread` без atomic | ?? Критично | Низкая | Data race, UB |
| 4 | God Object GameInstance | ?? Высокий | Высокая | Невозможность тестирования |
| 5 | Монолитные ProcessData методы | ?? Высокий | Средняя | Баги при добавлении событий |
| 6 | JSON парсинг 3-5 раз на пакет | ?? Высокий | Средняя | Деградация при масштабе |
| 7 | Таймеры вместо событий в BeginPlay | ?? Средний | Средняя | Инициализационные баги |
| 8 | GetAllActorsOfClass в input | ?? Средний | Низкая | Фризы при много MOB |
| 9 | Отсутствие client prediction | ?? Средний | Высокая | Плохой геймфил |
| 10 | Дублирование InventoryManager | ?? Средний | Средняя | Утечки памяти |
| 11 | JSONParser::GWorld | ?? Низкий | Низкая | Проблемы в тестах |
| 12 | LogTemp/Warning везде | ?? Низкий | Низкая | Production overhead |

---

## Рекомендуемая целевая архитектура сетевого стека

```
[TCP Socket]
    ?
[NetworkReceiverWorker] — читает байты, собирает пакеты по length-prefix
    ? (TQueue<FRawPacket> — один раз аллоцированные буферы)
[Game Thread Poll — NetworkManager]
    ? — десериализация JSON/Binary ОДИН РАЗ
[MessageDispatcher] — диспатч по eventType через TMap<FString, TFunction>
    ?
[AuthHandler] [PlayerHandler] [CombatHandler] [NPCHandler] ...
```

Каждый Handler — небольшой класс с единственной ответственностью, тестируемый независимо.
