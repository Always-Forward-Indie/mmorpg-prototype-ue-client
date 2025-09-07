# Диагностика проблемы с иконками навыков

## Проблема

Иконки навыков не загружаются и используется fallback, хотя иконки есть в таблице данных.

## Возможные причины

### 1. SkillDefinitionRepository не инициализирован

**Проверка:** В логах должно появиться:
```
PlayerSkillManager: DefinitionRepository is NULL
```

**Решение:** Убедиться, что `SkillDefinitionRepository` инициализирован в `MyGameInstance`:

```cpp
// В MyGameInstance::Initialize()
if (SkillDefinitionsDataTable)
{
    USkillDefinitionRepository* DefinitionRepo = GetSkillDefinitionRepository();
    DefinitionRepo->Initialize(SkillDefinitionsDataTable);
}
```

### 2. DataTable не установлена

**Проверка:** В логах должно появиться:
```
SkillDefinitionRepository: Cannot refresh cache, DataTable is null
```

**Решение:** В Blueprint `MyGameInstance` установить `SkillDefinitionsDataTable`.

### 3. Навыки не найдены в таблице

**Проверка:** В логах должно появиться:
```
SkillDefinitionRepository: No definition found for [skill_name], returning default
```

**Решение:** Убедиться, что в DataTable есть строки с правильными `skillSlug`.

### 4. Иконки не установлены в таблице

**Проверка:** В логах должно появиться:
```
SkillDefinitionRepository: Found cached definition for [skill] - Icon IsValid: false
```

**Решение:** В DataTable установить иконки для каждого навыка в поле `skillIcon`.

### 5. Некорректные пути к иконкам

**Проверка:** В логах должен быть путь к иконке:
```
SkillDefinitionRepository: Icon path: /Game/...
```

**Решение:** Убедиться что иконки находятся по указанному пути.

## Пошаговая диагностика

### Шаг 1: Проверить инициализацию SkillDefinitionRepository

```cpp
// В MyGameInstance::Initialize()
UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Initializing SkillDefinitionRepository"));

USkillDefinitionRepository* DefinitionRepo = GetSkillDefinitionRepository();
if (!DefinitionRepo)
{
    UE_LOG(LogTemp, Error, TEXT("MyGameInstance: SkillDefinitionRepository is NULL"));
    return;
}

if (!SkillDefinitionsDataTable)
{
    UE_LOG(LogTemp, Error, TEXT("MyGameInstance: SkillDefinitionsDataTable is not set"));
    return;
}

DefinitionRepo->Initialize(SkillDefinitionsDataTable);
```

### Шаг 2: Проверить содержимое DataTable

В Unreal Editor:
1. Открыть `SkillDefinitionsDataTable`
2. Убедиться что есть строки для навыков (например, `power_slash`)
3. Проверить что поле `skillSlug` заполнено
4. Проверить что поле `skillIcon` установлено

### Шаг 3: Проверить инициализацию PlayerSkillManager

```cpp
// В MyGameInstance или где создается PlayerSkillManager
UPlayerSkillManager* SkillManager = GetPlayerSkillManager();
USkillDefinitionRepository* DefinitionRepo = GetSkillDefinitionRepository();

if (SkillManager && DefinitionRepo)
{
    SkillManager->Initialize(SkillSystemManager, DefinitionRepo, TimeSyncService);
}
```

## Ожидаемые логи при корректной работе

```
SkillDefinitionRepository: Loaded definition for skill power_slash
PlayerSkillManager: DefinitionRepository available, getting definition
SkillDefinitionRepository: Getting definition for skill power_slash
SkillDefinitionRepository: Found cached definition for power_slash - DisplayName: Power Slash, Icon IsValid: true
SkillDefinitionRepository: Icon path: /Game/UI/Icons/Skills/power_slash_icon
PlayerSkillManager: Loaded definition - DisplayName: Power Slash, Icon IsValid: true
SkillItemWidget: UpdateVisualDisplay for skill power_slash
SkillItemWidget: Skill icon IsValid: true, DisplayName: Power Slash
SkillItemWidget: Attempting to load skill icon from TSoftObjectPtr
SkillItemWidget: Successfully set skill icon from texture
```

## Логи при проблемах

### Если DefinitionRepository не инициализирован:
```
PlayerSkillManager: DefinitionRepository is NULL
SkillItemWidget: Skill icon IsValid: false, DisplayName: power_slash
SkillItemWidget: Skill icon TSoftObjectPtr is not valid
```

### Если навык не найден в таблице:
```
SkillDefinitionRepository: No definition found for power_slash, returning default
SkillItemWidget: Skill icon IsValid: false, DisplayName: power_slash
```

### Если иконка не установлена:
```
SkillDefinitionRepository: Found cached definition for power_slash - Icon IsValid: false
SkillItemWidget: Skill icon TSoftObjectPtr is not valid
```

## Быстрое решение

Если проблема критическая, можно добавить временный DefaultSkillIcon:

```cpp
// В USkillItemWidget
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Item Widget")
UTexture2D* DefaultSkillIcon;

// В UpdateVisualDisplay()
if (!bIconSet && DefaultSkillIcon)
{
    SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Using default icon"));
}
```

## Следующие шаги

1. Запустить игру и открыть окно навыков
2. Найти в логах сообщения от `SkillDefinitionRepository` и `PlayerSkillManager`
3. Определить на каком этапе теряются данные иконок
4. Применить соответствующее решение

После добавления логов проблема должна стать очевидной!