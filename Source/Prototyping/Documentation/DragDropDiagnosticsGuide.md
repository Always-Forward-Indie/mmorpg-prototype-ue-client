# Руководство по диагностике проблем Drag & Drop

## Проблема
Скиллы не перетаскиваются в слоты SkillBarWidget. Слоты не подсвечиваются при наведении и не принимают перетаскиваемые скиллы.

## Возможные причины

### 1. Canvas Panel как корневой элемент
**ОСНОВНАЯ ПРОБЛЕМА**: Canvas Panel может блокировать события drag & drop.

**Симптомы:**
- События NativeOnDragOver не вызываются
- Слоты не подсвечиваются при drag over
- Визуально drag работает, но drop не происходит

**Решение:**
- Замените корневой Canvas Panel на Border или UserWidget
- В UMG редакторе смените Root Widget с Canvas Panel на Border

### 2. Перекрытие виджетов
**Проблема**: Другие виджеты перекрывают слоты и блокируют события мыши.

**Симптомы:**
- Некоторые слоты работают, другие нет
- Hit-testing не достигает нужных виджетов

### 3. Неправильная видимость элементов
**Проблема**: Элементы имеют неправильные настройки Visibility.

**Симптомы:**
- Button внутри слота блокирует события
- Контейнеры не видны или недоступны для hit-testing

## Диагностические функции

Добавлены новые Blueprint-вызываемые функции для диагностики:

### В SkillBarWidget:

```cpp
// Проверка настройки drag-drop для всех слотов
UFUNCTION(BlueprintCallable, Category = "Debug")
void DebugCheckDragDropSetup();

// Проверка перекрытий виджетов и Canvas Panel проблем
UFUNCTION(BlueprintCallable, Category = "Debug") 
void DebugCheckWidgetOverlap();

// Детальная проверка hit-testing для слотов
UFUNCTION(BlueprintCallable, Category = "Debug")
void DebugTestSlotHitTestingDetailed();

// Симуляция drop без UI (для тестирования логики)
UFUNCTION(BlueprintCallable, Category = "Debug")
void DebugSimulateSkillDrop(int32 SlotIndex, const FString& SkillSlug);
```

## Пошаговая диагностика

### Шаг 1: Проверьте логирование в Output Log
Запустите игру и попробуйте перетащить скилл. Ищите сообщения:

```
=== SKILL DRAG DETECTED DEBUG START ===
=== DRAG OVER DEBUG START ===  
=== SKILL DROP DEBUG START ===
```

**Если не видите DRAG OVER сообщений** ? проблема с hit-testing/перекрытием.

### Шаг 2: Вызовите диагностические функции
В Blueprint или консоли вызовите:

```cpp
// Проверка общего состояния
SkillBarWidget->DebugCheckDragDropSetup()

// Проверка Canvas Panel проблем  
SkillBarWidget->DebugCheckWidgetOverlap()

// Детальная проверка hit-testing
SkillBarWidget->DebugTestSlotHitTestingDetailed()
```

### Шаг 3: Ищите предупреждения в логах

**Canvas Panel предупреждения:**
```
?? ROOT IS CANVAS PANEL - this can cause drag-drop issues!
?? SLOT ROOT IS CANVAS PANEL - potential drag-drop issue!
```

**Проблемы с Button:**
```
?? SkillButton is Visible - might block drag events
```

**Перекрытие виджетов:**
```
? OVERLAPPING with Slot[X]!
```

### Шаг 4: Тестирование без UI
Проверьте, работает ли логика без UI:

```cpp
// Должно работать если логика корректна
SkillBarWidget->DebugSimulateSkillDrop(0, "fireball_1")
```

## Исправления по результатам диагностики

### Проблема: Canvas Panel Root
**В UMG редакторе:**
1. Откройте SkillBarWidget Blueprint
2. В Hierarchy выберите Root (Canvas Panel)
3. Right Click ? Replace with ? Border
4. Настройте Border размеры и alignment

### Проблема: Button блокирует события
**В SkillSlotWidget:**
- Установите SkillButton Visibility = "Hit Test Invisible"
- Или обрабатывайте drag события на уровне слота, а не кнопки

### Проблема: Перекрытие слотов
**В UMG:**
- Проверьте Layout в SkillSlotsContainer
- Убедитесь что слоты не перекрываются  
- Используйте правильные Size и Padding настройки

### Проблема: Контейнер невидим
**В UMG:**
- Проверьте Visibility всех контейнеров
- SkillSlotsContainer должен быть Visible
- Parent виджеты должны быть Visible

## Настройки Visibility для Drag & Drop

### Рекомендуемые настройки:

```cpp
// Основные контейнеры
SkillBarWidget Root: Visible (Border, не Canvas!)
SkillSlotsContainer: Visible

// Внутри SkillSlotWidget  
SkillSlotWidget Root: Visible
SkillButton: Hit Test Invisible (не блокирует drag)
SkillIcon: Hit Test Invisible
DropHighlightBorder: Hidden (показывается при drag over)
```

## Тестирование исправлений

После внесения изменений:

1. **Проверьте логи** - должны появиться DRAG OVER сообщения
2. **Проверьте подсветку** - слоты должны подсвечиваться при drag over
3. **Тестируйте drop** - слоты должны принимать скиллы
4. **Повторите диагностику** - не должно быть предупреждений

## Дополнительные советы

### Альтернативы Canvas Panel:
- **Border** - лучший выбор для корневого элемента
- **Overlay** - если нужно наложение элементов
- **UserWidget** - минимальный вариант

### Отладочные hotkeys:
Добавьте в PlayerController hotkeys для быстрой диагностики:
- F1: DebugCheckDragDropSetup
- F2: DebugCheckWidgetOverlap  
- F3: DebugTestSlotHitTestingDetailed

### Визуальная отладка:
- Включите "Show Widget Bounds" в UMG для видимости границ
- Используйте разные цвета для DropHighlightBorder
- Временно увеличьте размеры слотов для тестирования

Следуя этому руководству, вы сможете точно определить и исправить проблемы с drag & drop в вашей системе скиллов.