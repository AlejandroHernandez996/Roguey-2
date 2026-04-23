# Game Tick Architecture

## Why a Separate Tick

UE5's native `AActor::Tick()` fires every rendered frame — typically 60–120× per second. OSRS-style gameplay runs at a fixed 0.6s cadence (like OSRS's 600ms game tick). Driving gameplay from `Tick()` would:

- Make combat/movement rate tied to frame rate
- Make deterministic multiplayer harder (authoritative tick must be frame-independent)
- Make timing-based mechanics (cooldowns counted in ticks, not seconds) messy

So gameplay runs on a separate timer. Native `Tick()` is reserved for visual interpolation only.

## The Timer

`ARogueyGameMode` owns a `FTimerHandle GameTickHandle` set up in `BeginPlay`:

```cpp
GetWorldTimerManager().SetTimer(GameTickHandle, this, &ARogueyGameMode::OnGameTick,
    RogueyConstants::GameTickInterval, /*bLoop=*/true);
```

`OnGameTick()` fires every 0.6 seconds, increments `TickIndex`, and calls `RogueyTick(TickIndex)` on every registered `IRogueyTickable`.

This timer is **server-only** — `ARogueyGameMode` exists only on the server/listen-server host.

## IRogueyTickable

```cpp
class IRogueyTickable
{
public:
    virtual void RogueyTick(int32 TickIndex) = 0;
};
```

Any `UObject` (or `AActor`) that needs gameplay-tick access implements this interface and calls `GameMode->RegisterTickable(this)` to be added to the tick list.

## Registered Tickables and Order

Managers are created and registered in `ARogueyGameMode::InitGame` (before BeginPlay, before actors spawn). Tick order matters:

1. **URogueyGridManager** — processes any pending tile-type changes
2. **URogueyMovementManager** — resolves queued moves, calls `CommitMove`
3. **URogueyActionManager** — dispatches queued actions (attack, interact)
4. **URogueyNpcManager** — AI decisions (aggro, wander, leash, return)
5. **URogueyCombatManager** — stateless; called by ActionManager, not ticked directly
6. **URogueyDeathManager** — removes dead pawns after all other managers have run

## Hard Rules

- **No gameplay logic in `Tick()`**. If it affects game state (HP, position, combat), it belongs in `RogueyTick()` or called from a manager.
- **No `FTimerHandle` for gameplay**. Use `TickIndex` arithmetic instead (`LastAttackTick + CooldownTicks <= TickIndex`).
- `TickIndex` starts at 0 and increments every 0.6s. It is the universal clock for cooldowns, regen intervals, and anything time-based.

## What Is Allowed in Native Tick

- `ARogueyPawn::Tick()` — drains `TrueTileQueue` and calls `SetActorLocation()` for smooth visual interpolation
- `ARogueyPlayerController::PlayerTick()` — hover highlighting, camera movement

Both are purely cosmetic and do not touch game state.
