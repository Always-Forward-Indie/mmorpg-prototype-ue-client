# Система Тултипов для Навыков - План Реализации

## ?? Цель
Реализовать систему тултипов для навыков, аналогичную той, что используется в инвентаре. При наведении на навык должен появляться подробный тултип с описанием и характеристиками, а при начале перетаскивания - скрываться.

## ??? Реализованные Компоненты

### 1. **USkillTooltipWidget** ?
**Файлы:** 
- `Source/Prototyping/Public/UI/SkillTooltipWidget.h`
- `Source/Prototyping/Private/UI/SkillTooltipWidget.cpp`

**Функциональность:**
- Отображение детальной информации о навыке
- Автоматическое позиционирование (не выходит за границы экрана)
- Цветовая кодировка школ магии и типов эффектов
- Показ иконки, уровня, описания, кулдауна, расхода маны, дальности
- Поддержка анимаций (fade in/out)

```cpp
// Основные методы
void SetSkillData(const FPlayerSkillData& SkillData);
void ShowTooltip();
void HideTooltip();
void UpdateTooltipPosition(FVector2D ScreenPosition);
```

### 2. **Расширенный UAvailableSkillsWidget** ?
**Файлы:** 
- `Source/Prototyping/Public/UI/AvailableSkillsWidget.h`
- `Source/Prototyping/Private/UI/AvailableSkillsWidget.cpp`

**Добавленная функциональность:**
- Создание и управление тултипом
- Обработка hover событий от skill items
- NativeTick для обновления позиции тултипа
- Интеграция с системой тултипов

```cpp
// Новые методы
void OnSkillItemHovered(const FPlayerSkillData& SkillData, bool bIsHovered);
void ShowSkillTooltip(const FPlayerSkillData& SkillData, FVector2D Position);
void HideSkillTooltip();
void UpdateTooltipPosition();
```

### 3. **Обновленный USkillItemWidget** ?
**Файлы:** Изменения в `Source/Prototyping/Private/UI/AvailableSkillsWidget.cpp`

**Добавленная функциональность:**
- Новый делегат `OnSkillItemHovered` для hover событий
- Упрощенное отображение (только иконка + уровень)
- Автоматическое скрытие тултипа при drag
- Broadcast hover событий в `NativeOnMouseEnter/Leave`

```cpp
// Новый делегат
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSkillItemHovered, const FPlayerSkillData&, SkillData, bool, bIsHovered);

// В классе USkillItemWidget
UPROPERTY(BlueprintAssignable, Category = "Skill Item Widget")
FSkillItemHovered OnSkillItemHovered;
```

## ?? Настройка UMG Blueprint

### Создание SkillTooltipWidget Blueprint:

1. **Создать Blueprint виджет** наследованный от `USkillTooltipWidget`
2. **Настроить иерархию виджетов:**

```
SkillTooltipWidget (UUserWidget)
??? TooltipBorder (Border)
    ??? MainContent (VerticalBox)
        ??? HeaderBox (HorizontalBox)
        ?   ??? SkillIcon (Image)
        ?   ??? HeaderInfo (VerticalBox)
        ?       ??? SkillNameText (TextBlock)
        ?       ??? SkillSchoolText (TextBlock)
        ?       ??? SkillLevelText (TextBlock)
        ??? Separator1 (Image)
        ??? SkillDescriptionText (TextBlock)
        ??? Separator2 (Image)
        ??? StatsBox (VerticalBox)
            ??? CooldownText (TextBlock)
            ??? ManaCostText (TextBlock)
            ??? DamageText (TextBlock)
            ??? RangeText (TextBlock)
            ??? EffectTypeText (TextBlock)
```

3. **Привязать компоненты** используя **Bind Widget** для всех элементов
4. **Настроить стили** (шрифты, цвета, отступы)

### Настройка AvailableSkillsWidget:

1. **Установить SkillTooltipWidgetClass** в Blueprint на созданный SkillTooltipWidget
2. **Убедиться что SkillItemWidgetClass** настроен корректно

### Упрощение SkillItemWidget:

1. **Скрыть ненужные элементы** в SkillItemWidget Blueprint:
   - SkillNameText (Visibility = Collapsed)
   - SkillDescriptionText (Visibility = Collapsed) 
   - CooldownText (Visibility = Collapsed)
   - ManaCostText (Visibility = Collapsed)

2. **Оставить видимыми:**
   - SkillIcon (основная иконка)
   - SkillLevelText (уровень навыка)
   - SkillTypeIndicator (цветовой индикатор школы)

## ?? Поток Событий

### При наведении курсора на навык:
```
1. USkillItemWidget::NativeOnMouseEnter()
2. OnSkillItemHovered.Broadcast(SkillData, true)
3. UAvailableSkillsWidget::OnSkillItemHovered()
4. ShowSkillTooltip() -> создает/показывает тултип
5. NativeTick() -> обновляет позицию тултипа
```

### При начале перетаскивания:
```
1. USkillItemWidget::NativeOnDragDetected()
2. OnSkillItemHovered.Broadcast(SkillData, false) // скрываем тултип
3. UAvailableSkillsWidget::OnSkillItemHovered()
4. HideSkillTooltip() -> скрывает тултип
```

### При уходе курсора:
```
1. USkillItemWidget::NativeOnMouseLeave()
2. OnSkillItemHovered.Broadcast(SkillData, false)
3. UAvailableSkillsWidget::OnSkillItemHovered()
4. HideSkillTooltip() -> скрывает тултип
```

## ?? Цветовая Схема

### Школы Магии:
- **Physical** - Коричневый (0.8, 0.4, 0.2)
- **Fire** - Красный (1.0, 0.3, 0.0)
- **Ice** - Голубой (0.4, 0.8, 1.0)
- **Nature** - Зеленый (0.2, 0.8, 0.2)
- **Arcane** - Фиолетовый (0.6, 0.2, 1.0)
- **Shadow** - Темно-фиолетовый (0.3, 0.1, 0.5)
- **Holy** - Золотой (1.0, 1.0, 0.3)

### Типы Эффектов:
- **Damage** - Красный (1.0, 0.2, 0.2)
- **Healing** - Зеленый (0.2, 1.0, 0.2)
- **Buff** - Синий (0.2, 0.6, 1.0)
- **Debuff** - Оранжевый (1.0, 0.6, 0.0)
- **Resource** - Серый (0.8, 0.8, 0.8)

## ?? Отображаемая Информация

### В тултипе показывается:
1. **Заголовок:**
   - Иконка навыка
   - Название (цвет школы магии)
   - Школа магии (цвет школы)
   - Уровень навыка

2. **Описание:**
   - Текстовое описание навыка

3. **Характеристики:**
   - Кулдаун (в секундах/минутах)
   - Расход маны (если есть)
   - Дамаг (показывает "Variable" если нет точных данных)
   - Дальность (метры или "Melee")
   - Тип эффекта (цвет типа)

### В упрощенном skill item:
- Только иконка навыка
- Уровень навыка (в формате "Lv.X")
- Цветовой индикатор школы магии

## ?? Следующие Шаги

1. **Создать UMG Blueprint** для SkillTooltipWidget
2. **Настроить привязки** всех UI элементов
3. **Установить класс тултипа** в AvailableSkillsWidget
4. **Упростить отображение** SkillItemWidget
5. **Протестировать** функциональность

## ?? Техническая Реализация

### Ключевые особенности:
- **Автоматическое позиционирование** - тултип не выходит за границы экрана
- **Z-order управление** - тултип всегда поверх других UI (Z-order = 1000)
- **Эффективная память** - один тултип на все навыки
- **Интеграция с drag & drop** - автоматическое скрытие при перетаскивании
- **Цветовая кодировка** - визуальное различение школ и типов

### Производительность:
- Lazy loading иконок навыков
- Кэширование цветов школ и типов
- Один тултип для всех навыков (не создается отдельно для каждого)
- Обновление позиции только когда тултип видим

## ? Результат

После реализации пользователь получит:
- **Информативный тултип** при наведении на навык
- **Чистый интерфейс** skill items (только иконка + уровень)
- **Интуитивное поведение** - тултип исчезает при drag операциях
- **Визуальные подсказки** через цветовое кодирование
- **Отзывчивый UI** - тултип следует за курсором и не выходит за экран