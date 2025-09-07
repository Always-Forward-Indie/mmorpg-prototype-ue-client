# Система Drag-and-Drop для Скилов

## Обзор

Реализована система перетаскивания скилов из окна доступных скилов (AvailableSkillsWidget) в слоты панели скилов (SkillBarWidget). Система позволяет игрокам легко назначать скилы на панель действий.

## Как это работает

### 1. Перетаскивание скилов

- Откройте окно доступных скилов (AvailableSkillsWidget)
- Нажмите и удерживайте левую кнопку мыши на любом скиле
- Перетащите скил в желаемый слот на панели скилов
- Отпустите кнопку мыши для размещения скила

### 2. Визуальная обратная связь

- При наведении курсора на скил в списке доступных скилов - скил подсвечивается
- При перетаскивании скила над подходящим слотом - слот подсвечивается зеленым цветом
- При отпускании скила над неподходящим местом - операция отменяется

### 3. Автоматическое назначение горячих клавиш

- При размещении скила в слот автоматически назначается горячая клавиша
- По умолчанию используются клавиши 1-9 и 0 для слотов 0-9
- Можно настроить собственные горячие клавиши

## Технические детали

### Основные компоненты

1. **USkillDragDropOperation** - класс операции перетаскивания
   - Содержит данные перетаскиваемого скила
   - Управляет визуальной обратной связью

2. **USkillItemWidget** - виджет элемента скила в списке
   - Поддерживает начало операции перетаскивания
   - Обрабатывает события мыши для инициации drag

3. **USkillSlotWidget** - виджет слота скила на панели
   - Принимает операции drop
   - Показывает визуальную обратную связь при наведении
   - Проверяет возможность размещения скила

4. **USkillBarWidget** - основная панель скилов
   - Обрабатывает событие размещения скила
   - Вызывает назначение скила через SkillManager

### События и делегаты

- `FOnSkillDroppedOnSlot` - событие размещения скила в слот
- `FSkillSlotClicked` - клик по слоту скила
- `FSkillSlotRightClicked` - правый клик по слоту скила

### Методы для Blueprint

#### USkillSlotWidget
- `SetDropHighlighted(bool)` - подсветка слота при drag-over
- `CanAcceptSkillDrop()` - проверка возможности размещения

#### USkillBarWidget
- `AssignSkillToSlot(SlotIndex, SkillSlug, Hotkey)` - программное назначение скила

## Настройка в Blueprint

### 1. Настройка AvailableSkillsWidget

```cpp
// В Blueprint установите DragDropOperationClass
DragDropOperationClass = USkillDragDropOperation::StaticClass();
```

### 2. Настройка SkillBarWidget

Убедитесь что:
- `SkillSlotsContainer` или `SkillGridContainer` настроены
- `SkillSlotWidgetClass` указывает на класс USkillSlotWidget
- Подписаны на события OnSkillDroppedOnSlot

### 3. Настройка UI виджетов

#### Для SkillSlotWidget в UMG:
- Добавьте опциональный Image виджет с именем "DropHighlightBorder"
- Настройте цвета подсветки в классе

#### Для SkillItemWidget в UMG:
- Убедитесь что виджет имеет Button компонент для обработки мыши

## Расширения и кастомизация

### Добавление валидации

Можно расширить метод `CanAcceptSkillDrop()` для дополнительных проверок:

```cpp
bool USkillSlotWidget::CanAcceptSkillDrop(USkillDragDropOperation* DragDropOp) const
{
    // Базовые проверки
    if (!DragDropOp || !SkillManager) return false;
    
    // Проверка уровня скила
    if (DragDropOp->SkillData.networkData.skillLevel < RequiredLevel)
        return false;
        
    // Проверка типа скила
    if (!IsSkillTypeAllowed(DragDropOp->SkillData.definitionData.effectType))
        return false;
        
    return true;
}
```

### Кастомизация визуальной обратной связи

Можно настроить цвета и эффекты в классе:

```cpp
UPROPERTY(EditAnywhere)
FLinearColor DropHighlightColor = FLinearColor::Green;

UPROPERTY(EditAnywhere) 
FLinearColor InvalidDropColor = FLinearColor::Red;
```

## Отладка

### Логирование

Система включает детальное логирование:
- Начало и завершение операций drag-drop
- Проверки валидации
- События размещения скилов

### Общие проблемы

1. **Скил не перетаскивается**
   - Проверьте что `DragDropOperationClass` установлен
   - Убедитесь что Button виджет корректно настроен

2. **Слот не принимает скил**
   - Проверьте логику в `CanAcceptSkillDrop()`
   - Убедитесь что SkillManager инициализирован

3. **Нет визуальной обратной связи**
   - Проверьте наличие `DropHighlightBorder` виджета
   - Убедитесь что цвета подсветки настроены

## Будущие улучшения

- Анимации перетаскивания
- Звуковая обратная связь
- Перетаскивание между слотами
- Контекстные меню для управления скилами
- Группировка скилов по категориям