# Experience Widget Blueprint Integration Guide

## Быстрая интеграция UPlayerExperienceWidget в Blueprint

### 1. Создание Blueprint виджета

1. **Создайте новый Widget Blueprint** в Content Browser
   - Щелкните правой кнопкой ? User Interface ? Widget Blueprint
   - Назовите его `WBP_PlayerExperience`

2. **Установите Parent Class** на `PlayerExperienceWidget`
   - В Details панели установите Parent Class = PlayerExperienceWidget

### 2. Настройка UI компонентов

Добавьте следующие компоненты в Designer режиме:

```
Canvas Panel (Root)
??? Horizontal Box (Main Container)
    ??? Text Block (LevelText) 
    ?   ??? Variable Name: "LevelText"
    ?   ??? Binding: "Level 1"
    ??? Progress Bar (ExperienceProgressBar)
    ?   ??? Variable Name: "ExperienceProgressBar" 
    ?   ??? Percent: 0.0
    ?   ??? Size: 200x20
    ??? Text Block (ExperienceText)
    ?   ??? Variable Name: "ExperienceText"
    ?   ??? Binding: "0 / 100"
    ??? Text Block (ExperienceGainText)
        ??? Variable Name: "ExperienceGainText"
        ??? Visibility: Hidden
        ??? Color: Gold
```

### 3. Настройка переменных

В **Variables** разделе убедитесь, что следующие переменные связаны:

- `ExperienceProgressBar` (Progress Bar)
- `LevelText` (Text Block)  
- `ExperienceText` (Text Block)
- `ExperienceGainText` (Text Block)

### 4. Настройка анимационных событий (опционально)

Если хотите анимации, создайте следующие события в Event Graph:

```blueprint
Event PlayExperienceGainAnimation
??? Input: Exp Gained (Integer)
??? Input: Reason (String)
??? ? Play Animation "ExpGainAnim"

Event PlayLevelUpAnimation  
??? Input: New Level (Integer)
??? ? Play Animation "LevelUpAnim"

Event PlayProgressBarUpdateAnimation
??? Input: New Percent (Float)
??? ? Play Animation "ProgressBarFillAnim"
```

### 5. Настройка UIManager в GameInstance Blueprint

1. **Откройте GameInstance Blueprint**
2. **Найдите ExperienceWidgetClass переменную**
3. **Установите значение** на ваш `WBP_PlayerExperience`

### 6. Конфигурация в Project Settings

В **Project Settings ? Game ? Game Instance Class:**
- Убедитесь, что установлен ваш GameInstance Blueprint

### 7. Тестирование

1. **Запустите игру**
2. **Система автоматически**:
   - Создаст виджет опыта
   - Подключит его к ExperienceManager
   - Начнет показывать обновления опыта от сервера

### 8. Настройка позиционирования

Для позиционирования виджета:

```cpp
// В Blueprint Event Graph
Event Begin Play
??? ? Get Widget from Viewport
    ??? ? Set Position in Viewport (X: 50, Y: 50)
    ??? ? Set Anchors (0.0, 0.0, 0.3, 0.1)
```

### 9. Стилизация

Рекомендуемые настройки стиля:

**Progress Bar:**
- Fill Color: Blue
- Background Color: Dark Gray
- Border Color: Light Gray

**Level Text:**
- Font Size: 16
- Color: Yellow
- Font: Bold

**Experience Text:**
- Font Size: 12  
- Color: White

**Experience Gain Text:**
- Font Size: 14
- Color: Gold
- Animation: Fade In/Out

### 10. Отладка

Для отладки добавьте логирование:

```blueprint
Event OnExperienceGained
??? Input: Experience Event
??? ? Print String (Experience Event.Experience Gained)
```

### Автоматическая интеграция

Система автоматически:
- ? Создает виджет при запуске игры
- ? Инициализирует с текущим CharacterID
- ? Подключается к ExperienceManager  
- ? **Загружает начальные данные опыта** из playerData персонажа (уровень, текущий опыт, опыт для следующего уровня)
- ? Обновляет UI при получении пакетов опыта
- ? Показывает анимации повышения уровня
- ? Отображает прогресс к следующему уровню
- ? **Синхронизирует данные** при изменении опыта игрока

### Дополнительные возможности

**Custom Events для Blueprint:**
- `OnExperienceGained` - когда получен опыт
- `OnLevelUp` - при повышении уровня
- `OnProgressionUpdated` - при обновлении прогресса

**Доступные функции в Blueprint:**
- `GetCurrentLevel()` - текущий уровень
- `GetCurrentExperience()` - текущий опыт
- `GetExperienceToNextLevelPercent()` - процент до следующего уровня
- `UpdateExperienceData()` - принудительное обновление данных опыта в ExperienceManager

### Ручное обновление данных (если нужно)

Если по какой-то причине нужно принудительно обновить данные опыта:

```blueprint
Event Custom Update Experience
??? ? Get Player Pawn
    ??? ? Cast to BasicPlayer
        ??? ? Update Experience Data
```

Система готова к использованию! ??