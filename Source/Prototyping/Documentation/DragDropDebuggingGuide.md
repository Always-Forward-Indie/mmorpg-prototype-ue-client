# Отладка Drag-and-Drop системы скилов

## Добавленные улучшения

### 1. Управление курсором и фокусом

**Проблема:** При открытии панели скилов курсор не отображался.

**Решение:** Добавлено автоматическое управление курсором в `UIManager::ToggleSkillsPanel()`:

```cpp
// При открытии панели скилов
PlayerController->bShowMouseCursor = true;
PlayerController->bEnableClickEvents = true;
PlayerController->bEnableMouseOverEvents = true;

FInputModeGameAndUI InputMode;
InputMode.SetWidgetToFocus(AvailableSkillsWidget->TakeWidget());
InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
InputMode.SetHideCursorDuringCapture(false);
PlayerController->SetInputMode(InputMode);
```

### 2. Автоматическая настройка DragDropOperationClass

**Проблема:** DragDropOperationClass мог быть не установлен.

**Решение:** Добавлена автоматическая инициализация в конструкторе `USkillItemWidget`:

```cpp
// Установка класса операции по умолчанию
DragDropOperationClass = USkillDragDropOperation::StaticClass();
```

### 3. Расширенное логирование

Добавлено подробное логирование во все ключевые компоненты:

- **USkillItemWidget:** Логирование событий мыши, создания drag операции
- **USkillDragDropOperation:** Логирование создания и настройки drag visual
- **USkillDragVisualWidget:** Логирование обновления визуального отображения
- **UIManager:** Логирование управления курсором и фокусом

## Что проверить в логах

### При открытии панели скилов

Ожидаемые сообщения:
```
UIManager: Skills panel opened - cursor enabled and focus set
AvailableSkillsWidget: NativeConstruct - Widget constructed and focus set
AvailableSkillsWidget: Populated X skills
SkillItemWidget: Constructor - DragDropOperationClass set to SkillDragDropOperation
SkillItemWidget: NativeConstruct called for skill [SkillName]
```

### При попытке перетаскивания

Ожидаемые сообщения:
```
SkillItemWidget: Mouse button down - Button: LeftMouseButton, Skill: [SkillName]
SkillItemWidget: Left mouse button detected, preparing for drag
SkillItemWidget: DRAG DETECTED for skill [SkillName]
SkillItemWidget: Creating DragDropOperation of class SkillDragDropOperation
SkillDragDropOperation: Constructor called
SkillDragDropOperation: SetSkillData called for skill [SkillName]
SkillDragDropOperation: CreateDefaultDragVisual called
SkillDragVisualWidget: Constructor called
SkillDragVisualWidget: SetSkillData called for skill [SkillName]
SkillItemWidget: Started dragging skill [SkillName]
```

## Возможные проблемы и решения

### 1. Курсор не появляется

**Проверка:** Найдите в логах сообщение "Skills panel opened - cursor enabled and focus set"

**Решения:**
- Убедитесь, что UIManager имеет валидную ссылку на PlayerController
- Проверьте, что ToggleSkillsPanel() действительно вызывается

### 2. Drag-and-drop не запускается

**Проверка:** Ищите сообщения "Mouse button down" и "DRAG DETECTED"

**Возможные причины:**
- Button компонент не настроен в UMG
- Виджет не получает события мыши
- DragDropOperationClass не установлен

**Решения:**
```cpp
// В Blueprint убедитесь, что:
1. SkillButton существует и настроен
2. SkillItemWidget имеет правильный Visibility (Visible, не Collapsed)
3. В родительском ScrollBox включен "Allow Children Handle Events"
```

### 3. Drag visual не отображается

**Проверка:** Ищите сообщения от SkillDragVisualWidget

**Возможные причины:**
- USkillDragVisualWidget не может быть создан
- Компоненты виджета (SkillIcon, SkillNameText) не привязаны

**Решения:**
- Создайте Blueprint виджет на основе USkillDragVisualWidget
- Привяжите компоненты через meta = (BindWidget)
- Установите DragVisualWidgetClass в USkillDragDropOperation

### 4. События мыши не работают в ScrollBox

**Решение в UMG:**
```
1. Выберите ScrollBox
2. В Details найдите "Consume Mouse Wheel" и установите в false
3. Включите "Allow Children Handle Events" в true
```

## Команды для отладки

### В консоли UE
```
log LogTemp Verbose  // Включить подробное логирование
log LogTemp Warning  // Только предупреждения и ошибки
log LogTemp Off      // Отключить логирование
```

### Проверка состояния курсора
```cpp
// В Blueprint или коде проверьте:
PlayerController->bShowMouseCursor
PlayerController->bEnableClickEvents
PlayerController->bEnableMouseOverEvents
```

## Пошаговая диагностика

### Шаг 1: Проверка основы
1. Запустите игру
2. Откройте панель скилов
3. Проверьте, появился ли курсор
4. Найдите в логах сообщения UIManager

### Шаг 2: Проверка создания виджетов
1. Найдите сообщения "SkillItemWidget: Constructor"
2. Убедитесь, что DragDropOperationClass установлен
3. Проверьте количество созданных skill item виджетов

### Шаг 3: Проверка событий мыши
1. Наведите мышь на скил
2. Найдите сообщения "Mouse entered skill"
3. Нажмите левую кнопку мыши
4. Найдите "Mouse button down - Button: LeftMouseButton"

### Шаг 4: Проверка drag операции
1. Попробуйте перетащить скил
2. Найдите "DRAG DETECTED"
3. Проверьте создание DragDropOperation
4. Убедитесь, что создался drag visual

## Частые ошибки

### 1. "DragDropOperationClass not set"
```cpp
// Решение: В конструкторе USkillItemWidget
DragDropOperationClass = USkillDragDropOperation::StaticClass();
```

### 2. "SkillButton is NULL"
```cpp
// Решение: В UMG Blueprint привяжите Button компонент с именем "SkillButton"
```

### 3. "PlayerController not available"
```cpp
// Решение: Убедитесь, что UIManager::Init() вызывается с валидным PlayerController
```

### 4. События мыши не доходят до виджетов
```cpp
// Решение: Проверьте иерархию виджетов и их Visibility
// Убедитесь, что родительские контейнеры не блокируют события
```

## Финальная проверка

После применения всех исправлений вы должны видеть:
1. ? Курсор появляется при открытии панели скилов
2. ? Скилы подсвечиваются при наведении мыши
3. ? При зажатии и перетаскивании появляется drag visual
4. ? Слоты подсвечиваются при наведении drag операции
5. ? Скилы успешно назначаются в слоты при drop операции

При возникновении проблем сначала проверьте логи, затем следуйте пошаговой диагностике!