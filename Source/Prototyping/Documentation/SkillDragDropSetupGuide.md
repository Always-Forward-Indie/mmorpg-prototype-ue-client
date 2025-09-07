# Система Drag & Drop для скилов - Руководство по настройке

## Обзор

Система позволяет игрокам перетаскивать скилы из панели доступных скилов в слоты на панели навыков. Реализована с использованием стандартной UE5 drag & drop системы с поддержкой визуального feedback'а.

## Архитектура

### Основные классы:

1. **USkillItemWidget** - Виджет отдельного скила в списке доступных
2. **USkillDragDropOperation** - Операция перетаскивания
3. **USkillDragVisualWidget** - Визуальное представление при перетаскивании
4. **USkillSlotWidget** - Слот для размещения скила на панели
5. **UAvailableSkillsWidget** - Панель со всеми доступными скилами
6. **USkillBarWidget** - Панель с слотами для скилов

## Настройка в Blueprint

### 1. USkillItemWidget Blueprint

Создайте Blueprint на основе `USkillItemWidget` с компонентами:

**Обязательные элементы (meta = "BindWidget"):**
- **SkillBorder** (UBorder) - основная граница для визуального feedback'а
- **SkillIcon** (UImage) - иконка скила
- **SkillNameText** (UTextBlock) - название скила
- **SkillDescriptionText** (UTextBlock) - описание скила
- **SkillLevelText** (UTextBlock) - уровень скила
- **CooldownText** (UTextBlock) - время перезарядки
- **ManaCostText** (UTextBlock) - стоимость маны
- **SkillTypeIndicator** (UBorder) - индикатор школы магии

**Настройки виджета:**
- Visibility: Visible
- Is Focusable: true (в Details panel)

### 2. USkillDragVisualWidget Blueprint

Создайте Blueprint на основе `USkillDragVisualWidget`:

**Структура:**
```
RootWidget (SizeBox)
??? DragBorder (Border)
    ??? VBox (VerticalBox)
        ??? SkillIcon (Image)
        ??? SkillNameText (TextBlock)
```

**Имена компонентов:**
- **SkillIcon** - для иконки скила
- **SkillNameText** - для названия скила
- **DragBorder** - для полупрозрачной подложки

### 3. USkillSlotWidget Blueprint

Создайте Blueprint на основе `USkillSlotWidget` с компонентами:

**Обязательные элементы:**
- **SkillButton** (Button) - основная кнопка слота
- **SkillIcon** (Image) - иконка назначенного скила
- **CooldownOverlay** (Image) - overlay для отображения кулдауна
- **CooldownProgress** (ProgressBar) - прогресс-бар кулдауна
- **CooldownText** (TextBlock) - текст оставшегося времени
- **HotkeyText** (TextBlock) - горячая клавиша
- **HighlightBorder** (Image) - подсветка при выделении

**Опциональные элементы:**
- **DropHighlightBorder** (Image) - подсветка при наведении drag'а

### 4. UAvailableSkillsWidget Blueprint

**Обязательные элементы:**
- **SkillListContainer** (ScrollBox) - контейнер для списка скилов
- **SkillCountText** (TextBlock) - счетчик скилов

**Настройки ScrollBox:**
- Allow Children Handle Events: true
- Consume Mouse Wheel: false

### 5. USkillBarWidget Blueprint

**Обязательные элементы:**
- **SkillSlotsContainer** (HorizontalBox) - контейнер для слотов

**Опциональные элементы:**
- **SkillGridContainer** (UniformGridPanel) - для сеточного расположения

## Настройка в C++

### 1. UIManager

Убедитесь, что в `ToggleSkillsPanel()` правильно настроен input mode:

```cpp
void UUIManager::ToggleSkillsPanel()
{
    if (bSkillsPanelVisible)
    {
        // При открытии панели скилов
        PlayerController->bShowMouseCursor = true;
        PlayerController->bEnableClickEvents = true;
        PlayerController->bEnableMouseOverEvents = true;
        
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(AvailableSkillsWidget->TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        PlayerController->SetInputMode(InputMode);
    }
}
```

### 2. Привязка классов

В Blueprint классах установите следующие ссылки:

**UAvailableSkillsWidget:**
- SkillItemWidgetClass = YourSkillItemWidget_BP

**USkillDragDropOperation:**
- DragVisualWidgetClass = YourSkillDragVisualWidget_BP

**USkillBarWidget:**
- SkillSlotWidgetClass = YourSkillSlotWidget_BP

## Процесс Drag & Drop

### 1. Начало перетаскивания:
1. Игрок нажимает ЛКМ на USkillItemWidget
2. `NativeOnMouseButtonDown` возвращает `FReply::Handled().DetectDrag()`
3. При движении мыши на достаточное расстояние вызывается `NativeOnDragDetected`
4. Создается `USkillDragDropOperation` с визуальным представлением

### 2. Процесс перетаскивания:
1. Игрок видит иконку скила под курсором
2. При наведении на `USkillSlotWidget` вызывается `NativeOnDragEnter`
3. Слот подсвечивается зеленым цветом (DropHighlightBorder)

### 3. Завершение перетаскивания:
1. При отпускании ЛКМ над слотом вызывается `NativeOnDrop`
2. Проверяется, можно ли назначить скил (`CanAcceptSkillDrop`)
3. Вызывается событие `OnSkillDroppedOnSlot`
4. `USkillBarWidget` назначает скил в слот через `PlayerSkillManager`

## Диагностика проблем

### Проблема: Drag не начинается

**Проверьте:**
1. `PlayerController->bEnableClickEvents = true`
2. `PlayerController->bEnableMouseOverEvents = true`
3. Виджет имеет Visibility = Visible
4. Родительские контейнеры не блокируют события

### Проблема: Drag visual не отображается

**Проверьте:**
1. `DragVisualWidgetClass` установлен в `USkillDragDropOperation`
2. Blueprint USkillDragVisualWidget правильно настроен
3. Компоненты имеют правильные имена (`SkillIcon`, `SkillNameText`)

### Проблема: Drop не работает

**Проверьте:**
1. `USkillSlotWidget` имеет правильную реализацию `NativeOnDrop`
2. Событие `OnSkillDroppedOnSlot` правильно привязано
3. `PlayerSkillManager` инициализирован

## Логирование

Включите логирование для отладки:

```
log LogTemp Warning
```

**Ожидаемые сообщения:**

При начале drag:
```
SkillItemWidget: DRAG DETECTED for skill [SkillName]
SkillDragDropOperation: Created custom drag visual
```

При drop:
```
SkillSlotWidget: Dropped skill [SkillName] on slot [SlotIndex]
SkillBarWidget: Assigned skill [SkillName] to slot [SlotIndex]
```

## Горячие клавиши

Система автоматически назначает горячие клавиши 1-9, 0 для слотов 0-9.
Использование: нажатие клавиши активирует скил в соответствующем слоте.

## Расширение системы

Для добавления новых функций:

1. **Drag между слотами** - реализуйте drag detection в USkillSlotWidget
2. **Контекстное меню** - добавьте обработку ПКМ
3. **Группировка скилов** - расширьте систему фильтрации
4. **Анимации** - добавьте UMG анимации для переходов

## Требования к производительности

- Drag visual создается только при необходимости
- Используется объектный пулинг для skill item widgets
- Lazy loading иконок скилов
- Асинхронная загрузка ресурсов