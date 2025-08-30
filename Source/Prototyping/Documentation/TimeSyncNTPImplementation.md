# NTP-Based Time Synchronization Implementation

## Проблема
Исходная реализация TimeSyncService использовала упрощенную формулу RTT/2 для расчета network latency и time offset, что приводило к нестабильности offset'а в диапазоне от +300ms до -200ms при нагрузке (поток пакетов движения).

**Дополнительные проблемы обнаружены:**
1. **Потеря точности при целочисленном делении**: NTP вычисления в int64 с делением `/2` теряли 0.5ms каждый раз
2. **Отсутствие near-best фильтрации**: Принимались сэмплы с большими очередями на сервере
3. **EWMA без ограничения шага**: Редкие плохие сэмплы "перетягивали" тренд в минус

## Решение
Реализован улучшенный NTP-подобный алгоритм с:
- Высокоточными вычислениями на double
- Near-best фильтрацией сэмплов
- EWMA с ограничением шага изменения
- Улучшенной валидацией сэмплов

## Ключевые изменения

### 1. Высокоточные NTP вычисления
```cpp
int64 UTimeSyncService::CalculateNTPOffset(const FTimeSyncData& SyncData) const
{
    // Используем double для устранения систематической ошибки округления
    double ClientToServer = static_cast<double>(SyncData.ServerRecvMs - SyncData.ClientSendMs);
    double ServerToClient = static_cast<double>(SyncData.ServerSendMs - SyncData.ClientRecvMs);
    double PreciseOffset = (ClientToServer + ServerToClient) / 2.0;
    
    return static_cast<int64>(FMath::RoundToDouble(PreciseOffset));
}
```

### 2. Near-Best Sample Filtering
```cpp
bool UTimeSyncService::IsNearBestSample(EServerType ServerType, const FTimeSyncData& NewSample) const
{
    // Находим лучший (минимальный) RTT в последних 10 сэмплах
    float BestRTT = NewSample.RoundTripTimeMs;
    // ... поиск минимального RTT ...
    
    // Принимаем сэмпл только если он в пределах 20% от лучшего RTT
    float RTTThreshold = BestRTT * 1.2f;
    return NewSample.RoundTripTimeMs <= RTTThreshold;
}
```

### 3. EWMA с ограничением шага
```cpp
void UTimeSyncService::ApplyEWMAFiltering(EServerType ServerType, const FTimeSyncData& NewSample)
{
    // Ограничиваем изменение offset ±50ms за сэмпл
    float MaxOffsetStep = 50.0f;
    OffsetDelta = FMath::Clamp(OffsetDelta, -MaxOffsetStep, MaxOffsetStep);
    
    // Ограичиваем изменение latency ±20ms за сэмпл
    float MaxLatencyStep = 20.0f;
    LatencyDelta = FMath::Clamp(LatencyDelta, -MaxLatencyStep, MaxLatencyStep);
    
    // Применяем ограниченную EWMA
    ServerData->FilteredOffsetMs += Alpha * OffsetDelta;
    ServerData->FilteredLatencyMs += Alpha * LatencyDelta;
}
```

### 4. Улучшенная валидация сэмплов
```cpp
bool UTimeSyncService::IsSampleValid(const FTimeSyncData& SyncData) const
{
    // Дополнительная проверка: отношение server processing к RTT
    float ProcessingRatio = ServerProcessing / RTT;
    if (ProcessingRatio > 0.5f) // Обработка сервера не должна превышать 50% RTT
    {
        return false;
    }
    // ... остальные проверки ...
}
```

## Настраиваемые параметры

### Точность вычислений
- Используется `double` для NTP вычислений
- `FMath::RoundToDouble()` для финального округления

### Near-Best фильтрация
- Проверяется последние 10 сэмплов для определения лучшего RTT
- Принимаются сэмплы в пределах 120% от лучшего RTT

### EWMA с ограничениями
- `MaxOffsetStepMs = 50ms` - максимальное изменение offset за сэмпл
- `MaxLatencyStepMs = 20ms` - максимальное изменение latency за сэмпл
- `EWMASmoothingFactor` ограничен диапазоном [0.01, 0.5]

### Валидация качества
- `MaxValidRTTMs = 1000ms` - максимальный RTT
- `MaxValidServerProcessingMs = 100ms` - максимальное время обработки сервера
- `ProcessingRatio < 0.5` - отношение обработки сервера к RTT

## Исправленные проблемы

### ? Было: Дрейф offset'а в минус
**Причина**: Систематическая ошибка округления при `int64 / 2`
**Решение**: Высокоточ pr?cis