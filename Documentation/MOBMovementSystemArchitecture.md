# MOB Movement System Ч Architectural Summary

**Date:** 2025-01-14  
**Version:** 2.0 (Production-Ready, AAA-quality)  
**Components:** `MOBMovementComponent.cpp`, `MOBMovementComponent.h`, `MOBAnimInstance.cpp/.h`

---

## 1. Core Architecture

### Dead-Reckoning Model

—ервер **не отправл€ет** позицию на каждом кадре. ¬место этого клиент получает:
- **јвторитативную позицию** (`position`) раз в 0.3Ц40 секунд (зависит от state)
- **¬ектор скорости** (`velocity = {dirX, dirY, speed}`) Ч нормализованное направление ? скорость (units/sec)
- **Rotation** (`rotationZ`) Ч серверный угол поворота в радианах

** лиентска€ экстрапол€ци€:**
```cpp
ExtrapolatedPos += SmoothedVelocity * DeltaTime
```
 аждый кадр моб движетс€ по `SmoothedVelocity`.  огда приходит новый пакет:
1. ¬ычислить `DeadReckonedPos = AuthoritativePos + Velocity ? TransitTime`
2. —равнить с `ActorPos`
3. ≈сли drift > 40 units ? плавна€ коррекци€ (0.08Ц0.20 сек blend)
4. ≈сли drift > 400 units ? instant snap (lag spike)

---

## 2. Key Design Decisions

### 2.1. Velocity Ч Authoritative, Not Blended

**—тара€ ошибка:**
```cpp
ServerVelocity = Lerp(old, new, 0.25f); // ? WRONG
```
ѕри каждом пакете моб двигалс€ **не в ту сторону** 75% времени.

**ѕравильно:**
```cpp
ServerVelocity = NewServerVelocity; // ? Accept truth immediately
SmoothedVelocity = VInterpTo(SmoothedVelocity, ServerVelocity, DeltaTime, 12.f);
```
`ServerVelocity` Ч истина. `SmoothedVelocity` Ч дл€ визуала (плавный переход анимации).

---

### 2.2. Extrapolation Never Freezes

**—тара€ ошибка:**
```cpp
if (TimeSinceLastPacket <= 0.5f)  // ? ѕри патруле пакеты раз в 10Ц40 сек!
    ExtrapolatedPos += Velocity * DeltaTime;
```
ћоб замирал через 0.5 сек и сто€л 9.5Ц39.5 сек до следующего пакета.

**ѕравильно:**
```cpp
if (SmoothedVelocity.SizeSquared2D() > 1.f)  // ? ƒвижемс€ пока velocity != 0
    ExtrapolatedPos += SmoothedVelocity * DeltaTime;
```
`MaxExtrapolationSec` теперь = `1.5f` и используетс€ только дл€ **drift detection thresholds**, не останова движени€.

---

### 2.3. Rotation Handling

**“ри сценари€:**

1. **Moving:** rotate to face `SmoothedVelocity.GetSafeNormal2D()`
   - Constant angular speed `MovingRotationSpeed = 240 deg/sec`
   - Never instant snap Ч always `RInterpConstantTo`

2. **Stopped (no target):** rotate to `TargetServerRotation` (from packet)
   - Slower `IdleRotationSpeed = 180 deg/sec`
   - Ensures mob faces correct direction when standing idle

3. **Stopped (has target):** rotate to face target actor (`RotateTowardsTarget`)
   - Same `IdleRotationSpeed`
   - Only when `bEnableTargetTracking && !bIsMoving`

**Critical:** Movement rotation and target rotation **never fight** Ч movement wins when `bIsMoving`, target wins when idle.

---

### 2.4. Stop Deceleration

**—тара€ ошибка:**
```cpp
StopBlend = MinMoveSpeed * DeltaTime * 2.f; // = 580 * 0.016 * 2 = 18.56 units/frame
```
ћоб проскакивал финальную позицию за 1Ц2 фрейма.

**ѕравильно:**
```cpp
DecelerationSpeed = Clamp(Dist2D * 5.f, 50.f, 200.f);
```
ƒеселераци€ **пропорциональна рассто€нию**: близко к stop pos ? медленнее.
- Min 50 units/sec (не останавливаетс€ мгновенно)
- Max 200 units/sec (не слишком медленно издалека)
- ѕри Dist < 1 unit ? snap to exact pos

---

## 3. Combat State Handling

### Frozen States (2, 3, 4, 6)

```cpp
static bool IsFrozenState(int32 State) {
    return (State == 2 || State == 3 || State == 4 || State == 6);
}
```

| State | Name | Duration | Movement | Rotation |
|-------|------|----------|----------|----------|
| 2 | PREPARING_ATTACK | `castTime` | ? Frozen | ? Face target |
| 3 | ATTACKING | `swingMs` | ? Frozen | ? Face target |
| 4 | ATTACK_COOLDOWN | `cooldownMs` | ? Frozen | ? Face target |
| 6 | EVADING | 2 sec | ? Frozen | ? Static |

**During freeze:**
- Position correction **disabled** (actor stays where it is)
- `ServerVelocity = 0`, `SmoothedVelocity = 0`
- Target tracking **active** (mob rotates to face target while attacking)

**On exit:**
- Compare `ActorLocation` vs `LastAuthoritativePos`
- If drift > 5 units ? smooth position correction
- Server sends forced `mobMoveUpdate` with new velocity

---

### Moving States (0, 1, 5, 7)

| State | Name | Packet Interval | Speed Multiplier |
|-------|------|----------------|------------------|
| 0 | PATROLLING | 10Ц40 sec | 1.0? |
| 1 | CHASING | 0.3 sec | 1.5? |
| 5 | RETURNING | 0.15 sec | 1.0? |
| 7 | FLEEING | (same as RETURNING) | 1.0? |

**State 7 (FLEEING):**
- AnimInstance flag `bIsFleeing = true` ? Animation Blueprint plays panicked run cycle
- Movement logic identical to other moving states (dead-reckoning + extrapolation)

---

## 4. Packet Frequency & Minimum Move Distance

**Server logic:**
```
if (distanceSinceLastPacket >= 50 units)
    SendMobMoveUpdate();
```

| Combat State | Expected Frequency | Why |
|--------------|-------------------|-----|
| PATROLLING | 10Ц40 sec | Slow random walk, rarely moves >50 units/tick |
| CHASING | ~0.3 sec | Fast movement (baseSpeedMax ? 1.5 = 210 units/0.3s = 70 units > threshold) |
| RETURNING | ~0.15 sec | Faster tick rate, guaranteed movement toward spawn |
| FROZEN (2/3/4/6) | Force-send on state change | Even if distance = 0 |

**Client must extrapolate** during long silences (patrol). Never freeze after X seconds without packet.

---

## 5. Position Correction Tiers

```cpp
if (Error2D >= 400.f)       // Instant snap (severe lag / teleport)
else if (Error2D >= 40.f)   // Smooth correction (0.08Ц0.20 sec)
else                        // Acceptable drift, no correction
```

**Why 40 units threshold:**
- At 150 units/sec (typical chase speed), 40 units = ~0.25 sec of drift
- Below this Ч player won't notice (within acceptable visual tolerance)
- Above this Ч visible desync, correct it smoothly

**Correction blend uses `SmoothStep` alpha:**
```cpp
Alpha = SmoothStep(0.0f, 1.0f, Clamp(ElapsedTime / Duration, 0, 1));
CorrectedPos = Lerp(StartPos, TargetPos, Alpha);
```
Smooth acceleration ? constant ? smooth deceleration (no jerk at start/end).

---

## 6. Ground Adjustment

**Multi-trace ground detection:**
- Center trace + 4 cardinal directions (Forward, Back, Left, Right) when moving
- **Median height** (not average) ? immune to outliers (prevents pop on single bad trace)
- Priority system:
  - During correction: `Priority = 1.0`
  - During normal movement: `Priority = Clamp(1.0 - Dist/500, 0.3, 1.0)` (closer to target ? higher priority)
  - On spawn: `Priority = 2.0` (aggressive snap)

**Asymmetric interp speed:**
- **Falling (Z < 0):** `15.0 ? Priority` Ч faster (gravity feel)
- **Climbing (Z > 0):** `8.0 ? Priority ? UpFactor` Ч slower (prevents pop when stepping up)
- **Far from ground (|Z| > 100):** `? 2.0` Ч catch up quickly

---

## 7. Rotation Z Conversion

Server sends `rotationZ` in **radians** (Unreal standard for network packets).
Client converts to degrees for `FRotator`:
```cpp
TargetServerRotation = FRotator(0.f, RadiansToDegrees(rotationZ), 0.f);
```

**When to use:**
- **Idle:** lerp actor rotation to `TargetServerRotation` at `IdleRotationSpeed`
- **Moving:** ignore it, rotate to face `SmoothedVelocity` direction
- **Frozen:** face target (via `RotateTowardsTarget`), ignore server rotation

---

## 8. AnimInstance Integration

**Variables exposed to Animation Blueprint:**

| Variable | Type | Source | Purpose |
|----------|------|--------|---------|
| `Speed` | float | `MOBMovementComponent::GetCurrentSpeed()` | Blend-space (idle ? walk ? run) |
| `bIsMoving` | bool | `MOBMovementComponent::IsMoving()` | Enable locomotion state |
| `bIsFleeing` | bool | `MOBMovementComponent::IsFleeing()` | Panic run cycle |
| `bIsAttacking` | bool | Set by `StartAttack()` | Attack montage active |
| `bIsAggressive` | bool | Set by `combatInitiation` | Combat idle vs neutral idle |
| `bIsDead` | bool | Set by `mobDeath` | Death state (freeze on last frame) |
| `bIsHit` | bool | Set by `combatResult` | Hit-react blend |

**Animation Blueprint logic (pseudocode):**
```
State Machine:
  ?? Death (if bIsDead)
  ?? Hit React (if bIsHit)
  ?? Attack (if bIsAttacking)
  ?? Locomotion
      ?? Idle (Speed < 10)
      ?? Walk (Speed 10Ц100)
      ?? Run/Flee
          ?? Normal Run (Speed > 100 && !bIsFleeing)
          ?? Panic Run (Speed > 100 && bIsFleeing)
```

---

## 9. Performance Optimizations

**Target tracking throttle:**
```cpp
TimeSinceLastTargetUpdate += DeltaTime;
if (!CachedTargetActor.IsValid() || TimeSinceLastTargetUpdate >= 0.1f) {
    TimeSinceLastTargetUpdate = 0.0f;
    CachedTargetActor = FindTargetActor(CurrentTargetId, CurrentTargetType);
}
```
`GetAllActorsOfClass` only every 0.1 sec, not every tick (60 Hz ? 10 Hz).

**Ground check throttle (idle):**
```cpp
TimeSinceLastGroundCheck += DeltaTime;
if (TimeSinceLastGroundCheck >= 1.0f) {
    SnapToGround();
    TimeSinceLastGroundCheck = 0.0f;
}
```
When standing still: check terrain every 1 sec, not 60 Hz.

**Multi-trace only when moving:**
```cpp
if (bIsMoving) {
    // Add Forward/Back/Left/Right traces
}
```
Stationary mob ? 1 trace. Moving mob ? 5 traces.

---

## 10. Edge Cases Handled

### 10.1. Player Kiting During Frozen State

**Scenario:** Mob is attacking (frozen), player runs 50 units away.

**Old behavior:** Actor stayed at attack position, then teleported 50 units on combat state exit.

**New behavior:**
1. During frozen: `LastAuthoritativePos` keeps updating from packets
2. On exit: smooth correction from current pos to `LastAuthoritativePos` (0.15 sec blend)
3. Player sees mob **slide** to correct position, never teleport

---

### 10.2. Long Silence (Patrol)

**Scenario:** Patrol mob, packet every 30 sec.

**Old behavior:** Extrapolate 0.5 sec, then freeze for 29.5 sec. Looks broken.

**New behavior:** Extrapolate full 30 sec along `SmoothedVelocity`. When packet arrives, drift correction handles any accumulated error (usually < 40 units due to velocity smoothing).

---

### 10.3. Instant Direction Change

**Scenario:** Server sends `velocity = (1, 0)` then `velocity = (0, 1)` (90∞ turn).

**Old behavior:** Instant snap ? jerky rotation.

**New behavior:**
```cpp
SmoothedVelocity = VInterpTo(SmoothedVelocity, ServerVelocity, DeltaTime, 12.f);
```
`SmoothedVelocity` curves from (1,0) to (0,1) over ~0.1 sec (interp speed 12).
Rotation follows smoothed vector ? natural arc turn.

---

### 10.4. Stop Overshoot

**Scenario:** Mob moving at 150 units/sec, receives `velocity = 0`.

**Old behavior:** Harsh snap or overshoot by 10+ units.

**New behavior:**
```cpp
DecelerationSpeed = Clamp(Dist2D * 5.f, 50.f, 200.f);
```
- At 20 units away: 100 units/sec ? 0.2 sec to stop
- At 2 units away: 50 units/sec (clamped min) ? 0.04 sec to stop
- At 0.5 units away: snap to exact pos

---

## 11. Configuration Tunables

**In `MOBMovementComponent.h`:**

| Parameter | Default | Effect |
|-----------|---------|--------|
| `MaxAcceleration` | 800 units/s? | (Unused in current impl Ч legacy) |
| `MinMoveSpeed` | 580 units/sec | Fallback for old `zoneMoveMobs` packets without velocity |
| `SnapDistance` | 10 units | (Unused Ч replaced by deceleration curve) |
| `MovingRotationSpeed` | 240 deg/sec | How fast mob turns while walking |
| `IdleRotationSpeed` | 180 deg/sec | How fast mob turns while standing |
| `MaxExtrapolationSec` | 1.5 sec | Drift detection threshold (not movement freeze) |
| `bDebugGroundAdjustment` | false | Draw debug lines for terrain traces |

**Correction thresholds (hard-coded):**
- `< 5 units`: no correction
- `5Ц40 units`: no correction (acceptable tolerance)
- `40Ц400 units`: smooth correction (0.08Ц0.20 sec)
- `> 400 units`: instant snap

---

## 12. Testing Checklist

- [x] Patrol: mob moves smoothly between waypoints, no freezing after 1 sec
- [x] Chase: mob follows player with 0.3 sec packet rate, smooth tracking
- [x] Attack sequence: mob freezes during cast/swing/cooldown, faces target, resumes chase
- [x] Leash: mob runs back to spawn (RETURNING), regenerates HP, enters EVADING, then resumes patrol
- [x] Fleeing: mob runs away from player when low HP, `bIsFleeing` flag active
- [x] Rotation: smooth turns during movement, faces correct direction when idle
- [x] Stop: natural deceleration to exact position, no overshoot
- [x] Lag spike: instant snap recovery for >400 unit desync
- [x] Small drift: smooth correction for 40Ц400 unit desync
- [x] Ground conformance: mob doesn't float/clip on uneven terrain
- [x] Target lock-on: mob rotates to face target while standing/attacking

---

## 13. Known Limitations

1. **No prediction for server-side obstacles.** If server pathfinding makes mob detour around a rock, client will extrapolate straight through it until next packet arrives. This is acceptable Ч client is presentation-only, server is authoritative.

2. **No collision with other mobs.** Server handles mob-mob deflection, client just follows packets. If two mobs overlap visually for 0.3 sec between packets, this is expected.

3. **Assumes flat Z within dead-reckoning window.** `DeadReckonedPos` uses `ActorZ`, not `MoveEntry.positionZ`. This prevents z-fighting when mob is on a slope, but means extreme z-delta (> 10 units/0.3sec) might cause brief desync.

---

## 14. Future Enhancements

**Short-term:**
- [ ] Add `bDebugDeadReckoning` to visualize extrapolated position vs actor position
- [ ] Expose correction thresholds (40 / 400) as UPROPERTY for designer tuning
- [ ] Add rotation interpolation during correction (currently only position)

**Long-term:**
- [ ] Client-side obstacle avoidance (raycast ahead, slow down if wall detected)
- [ ] Predict server velocity changes based on terrain slope (uphill = slower)
- [ ] Synchronized animation timings with server combat ticks (hitbox-perfect)

---

**END OF DOCUMENT**
