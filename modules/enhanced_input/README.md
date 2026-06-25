# Enhanced Input (`enhanced_input`)

A Godot 4.7+ custom module that ports the UE5 Enhanced Input model: decouple
*what you mean* (an `EIAction`) from *how the player triggers it* (an
`EIMappingContext` of `InputEvent`s), then layer modifiers (deadzone, normalize,
…) and triggers (press, hold, tap, chord, …) on top.

It is the same idea as UE5 EI / Rewired / Unity's new Input System, but built
natively inside the Godot engine so the data model lives in C++ and is
inspector-friendly.

> Module status: **P1–P6 complete**, P7 (custom inspector plugin) wired up
> (registered as an `EditorPlugin`). P8 (polish / examples / docs) is still
> open. See [Implementation phases](#implementation-phases).

---

## Table of contents

1. [Why](#why)
2. [Build](#build)
3. [Project setup](#project-setup-autoload)
4. [Quick start](#quick-start)
5. [Core concepts](#core-concepts)
6. [API reference](#api-reference)
7. [Migration from `InputMap` (`EIBridge`)](#migration-from-inputmap-eibridge)
8. [Testing](#testing)
9. [Implementation phases](#implementation-phases)
10. [Spec reference](#spec-reference)

---

## Why

| You want…                       | Built-in `InputMap`                  | Enhanced Input                              |
| ------------------------------- | ------------------------------------ | ------------------------------------------- |
| Multiple bindings per action    | yes (key OR key OR key)              | yes, with per-mapping modifiers/triggers    |
| Deadzone / normalize on a stick | hand-rolled in code                  | drop a modifier into the chain              |
| "Hold to fire" / "double tap"   | hand-rolled in code                  | drop a `EITriggerHold` / `EITriggerDoubleTap` |
| Input contexts (Menu vs Game)   | none — you branch on game state yourself | priority-ordered `EIMappingContext` stack |
| Read input value as float       | `Input.get_action_strength(...)`     | typed `EIValue` (bool / 1D / 2D / 3D)        |
| Re-bind controls at runtime     | edit `InputMap` (works, awkward)     | swap an `.tres` IMC                          |
| Save a complete input setup     | not first-class                      | `.tres` for `EIAction` and `EIMappingContext` |

The trade-off: it is a custom module (you build Godot yourself), and the
inspector plugin (P7) is not done yet — for now you author `.tres` files
manually.

---

## Build

The module lives in `modules/enhanced_input/`, so the standard Godot SCons
build picks it up automatically.

```bash
# from the Godot source root
python -m SCons platform=<your_platform> target=editor dev_build=yes tests=yes -j$(nproc)
```

Common platforms: `windows`, `linux`, `macos`, `android`, `web`.

`tests=yes` is required if you want to run the doctest suite. `dev_build=yes`
gives you the `godot.<platform>.editor.dev.<arch>.console.exe` binary that
prints to stdout (handy for running tests).

If you only need to ship (no tests), `tests=no` is fine.

The module is unconditional — there is no `module_enhanced_input_enabled=yes`
flag. The `initialize_enhanced_input_module()` call is in
`register_module_types.gen.h`, generated automatically by SCons.

---

## Project setup (autoload)

The C++ `EISubsystem` is `Node`-based and needs a parent in the `SceneTree`
to avoid `print_orphan_nodes()` warnings. Drop a small autoload that
instantiates it and exposes it as a singleton:

### 1. Create `res://ei_autoload.gd`

```gdscript
# res://ei_autoload.gd
extends Node

func _ready() -> void:
    # Defensive: if the module wasn't compiled into this engine
    # build (e.g. user dropped the script into a stock project),
    # `instantiate` returns null and `add_child(null)` would crash.
    if not ClassDB.class_exists("EISubsystem"):
        push_error("EnhancedInput module not built into this engine — cannot start EISubsystem.")
        return
    var sub := ClassDB.instantiate("EISubsystem")
    add_child(sub)  # so it lives in the tree and is freed on quit
    Engine.register_singleton("EISubsystem", sub)
```

### 2. Enable it in Project Settings

`Project → Project Settings → Autoload` → add `res://ei_autoload.gd` as
`EI` (or whatever name you want).

Now `Engine.get_singleton("EISubsystem")` returns the live subsystem from
anywhere. `EIComponent` nodes also discover it via `EISubsystem::get_singleton()`.

> **Why an autoload shim and not a pure C++ singleton?** A free-floating
> C++ `Node` in `--test` mode shows up in `print_orphan_nodes()` and breaks
> GDScript tests that assert on that output. The autoload runs only in
> editor / runtime (not in `--test`), so the singleton is created exactly
> when there is a `SceneTree` to hold it. See the long comment at the top
> of `register_types.cpp` for the full rationale.

---

## Quick start

A 30-line end-to-end example that listens to "ui_accept" with a 0.2 s
hold trigger and a deadzone-free 1D axis "steer":

```gdscript
# res://player_input.gd
extends Node

var ia_accept: EIAction
var ia_steer:  EIAction
var ctx:       EIMappingContext

func _ready() -> void:
    # 1. Define actions.
    ia_accept = EIAction.new()
    ia_accept.value_type = EIAction.VALUE_TYPE_BOOL
    ia_accept.default_triggers = [EITriggerPressed.new()]  # fires once on press

    ia_steer = EIAction.new()
    ia_steer.value_type = EIAction.VALUE_TYPE_AXIS1D

    # 2. Define an IMC and add mappings.
    ctx = EIMappingContext.new()
    ctx.context_name = "Player"

    var key_space := InputEventKey.new()
    key_space.physical_keycode = KEY_SPACE
    ctx.add_mapping(key_space, ia_accept)

    var key_a := InputEventKey.new(); key_a.physical_keycode = KEY_A
    var key_d := InputEventKey.new(); key_d.physical_keycode = KEY_D
    ctx.add_mapping(key_a, ia_steer, [], [], false)  # consumes=false → layer with D
    ctx.add_mapping(key_d, ia_steer, [], [], false)

    # 3. Register the IMC with the subsystem.
    Engine.get_singleton("EISubsystem").add_mapping_context(ctx, 0)

    # 4. Subscribe via an EIComponent (auto-unbinds on _exit_tree).
    var comp := EIComponent.new()
    add_child(comp)
    comp.bind(ia_accept, EISubsystem.EI_TRIGGER_EVENT_TRIGGERED, _on_jump)

func _process(_dt: float) -> void:
    var v := Engine.get_singleton("EISubsystem").get_action_value_variant(ia_steer)
    # v is a Dictionary {type: "1D", x: float} — see "API reference" below.

func _on_jump() -> void:
    print("jump!")
```

Save that on any node (or as an autoload), press <kbd>Space</kbd>, watch the
console.

---

## Core concepts

### `EIAction` — what you mean

A `Resource` representing a logical input unit. Has:

| Field                | Type                       | Purpose                                                |
| -------------------- | -------------------------- | ------------------------------------------------------ |
| `value_type`         | enum (BOOL / AXIS1D / AXIS2D / AXIS3D) | What the sampler must produce               |
| `description`        | `String`                   | Free-form label shown in inspectors / logs             |
| `default_modifiers`  | `TypedArray<EIModifier>`   | Modifier chain applied after per-mapping modifiers     |
| `default_triggers`   | `TypedArray<EITrigger>`    | Trigger chain applied after per-mapping triggers       |

Actions are referenced by `Ref<EIAction>` from mappings, so the same action can
be reused across many contexts and saved as `.tres`.

### `EIMappingContext` — how the player triggers it

A `Resource` holding `InputEvent → EIAction` mappings. Each mapping has:

```gdscript
ctx.add_mapping(
    event,                # Ref<InputEvent>: the raw key/button/etc.
    action,               # Ref<EIAction>: the logical unit
    modifiers := [],      # per-mapping modifier chain (optional)
    triggers  := [],      # per-mapping trigger chain (optional)
    consumes  := true,    # if true, lower-priority contexts won't see this event
)
```

Contexts are registered with a **priority**:

```gdscript
subsystem.add_mapping_context(ctx, priority)   # higher = matched first
```

The dispatcher walks contexts in `(priority DESC, insertion order ASC)` order.
The first matching mapping in a context fires; if `consumes=true` the rest of
the chain stops; if `false`, lower-priority contexts also get to see the event
(useful for layered UI like a click-through HUD).

### Modifiers — transform the raw value

Concrete modifier subclasses (all in `modifiers/`):

| Class                       | What it does                                              |
| --------------------------- | --------------------------------------------------------- |
| `EIModifierDeadZone`        | radial (2D / 3D) or per-axis (1D) deadzone, with outer clamp |
| `EIModifierNegate`          | per-axis sign flip                                        |
| `EIModifierScale`           | per-axis scalar multiplier                                |
| `EIModifierNormalize`       | radial normalize (with min-length suppression)            |
| `EIModifierSwizzleAxis`     | reorder axes (e.g. swap X/Y, or XY→YZ for a Y-up stick)   |

Modifier chain order: per-mapping modifiers first, then action defaults.

### Triggers — decide when an action fires

Subclasses live in `triggers/`:

| Class                       | Event when…                                                       |
| --------------------------- | ----------------------------------------------------------------- |
| `EITriggerPressed`          | fires `STARTED` on press, `COMPLETED` on release                  |
| `EITriggerRelease`          | fires `TRIGGERED` on a non-zero → zero transition                 |
| `EITriggerHold` (`hold_time_threshold`)  | `STARTED` → `TRIGGERED` after threshold, `ONGOING` while held, `COMPLETED` on release |
| `EITriggerTap` (`tap_release_time`)| fires `TRIGGERED` if released within window, else `CANCELED`     |
| `EITriggerDoubleTap` (`double_tap_time`)| fires `TRIGGERED` on the second press within window              |
| `EITriggerPulse` (`pulse_interval`)  | fires `TRIGGERED` periodically while held                         |
| `EITriggerChord` (`chord_actions`)   | fires `TRIGGERED` when all chord actions are active               |

Trigger chain order: per-mapping triggers first, then action defaults. When
multiple triggers fire on the same frame, the priority aggregation rule from
spec §4.6 applies:

```
TRIGGERED > ONGOING > STARTED > COMPLETED > CANCELED > NONE
```

### `EIComponent` — bind and forget

A `Node` you add as a child of your gameplay node (player, UI, etc). It
forwards `bind(...)` / `unbind(...)` to the subsystem's
`bind_action()` / `unbind_action()` and auto-unbinds everything on
`_exit_tree` — no manual cleanup when an enemy despawns.

```gdscript
var comp := EIComponent.new()
add_child(comp)
comp.bind(jump_action, EISubsystem.EI_TRIGGER_EVENT_TRIGGERED, _on_jump)
```

---

## API reference

### `EISubsystem` (singleton, accessed via `Engine.get_singleton("EISubsystem")`)

| Method                                       | Returns      | Notes                                          |
| -------------------------------------------- | ------------ | ---------------------------------------------- |
| `add_mapping_context(ctx, priority)`         | void         | higher priority = matched first                |
| `remove_mapping_context(ctx)`                | void         | no-op if not registered                        |
| `clear_all_mapping_contexts()`               | void         |                                                |
| `has_mapping_context(ctx)`                   | bool         |                                                |
| `bind_action(action, event, callable)`       | void         | idempotent on (action, event, callable)        |
| `unbind_action(action, event, callable)`     | void         |                                                |
| `clear_bindings()`                           | void         |                                                |
| `inject_input(event)`                        | void         | programmatic dispatch                          |
| `tick(delta)`                                | void         | drives time-based triggers (runtime auto-ticks via internal process; tests call directly) |
| `get_action_value(action)` (C++)             | `EIValue`    | typed; see variant wrapper for GDScript        |
| `get_action_value_variant(action)` (gd)      | `Dictionary` | `{type, x?, y?, z?}`                           |
| `is_action_active(action)`                   | bool         | true while current value is non-zero           |
| `get_action_trigger_event(action)`           | `ETriggerEvent` | last event this action fired                |
| `set_log_level(level)` / `get_log_level()`   | int          | 0=silent, 1=event, 2=verbose                   |
| `is_application_focused()`                   | bool         | false → all in-progress triggers are canceled  |

`EIValue` shape (returned from C++):

| `value_type`     | Struct                |
| ---------------- | --------------------- |
| `BOOL`           | `{ bool b }`          |
| `AXIS1D`         | `{ float x }`         |
| `AXIS2D`         | `{ float x, float y }` |
| `AXIS3D`         | `{ float x, float y, float z }` |

### `EIAction` (Resource)

| Property                  | Type                              |
| ------------------------- | --------------------------------- |
| `value_type`              | `EIAction.ValueType` enum         |
| `description`             | `String`                          |
| `default_modifiers`       | `TypedArray<EIModifier>`          |
| `default_triggers`        | `TypedArray<EITrigger>`           |

### `EIMappingContext` (Resource)

| Method                                                            | Notes                              |
| ----------------------------------------------------------------- | ---------------------------------- |
| `add_mapping(event, action, modifiers?, triggers?, consumes?)`    | rejects null inputs silently       |
| `remove_mapping(event, action)`                                   |                                    |
| `clear_mappings()`                                                |                                    |
| `set_mappings(arr)`                                               | replace whole list                 |
| `get_mappings()`                                                  | snapshot of `Array<Dictionary>`    |
| `get_mapping_count()`                                             |                                    |
| `context_name`                                                    | free-form label                    |

### `EIBridge` (RefCounted, static methods)

| Method                                                            | Notes                              |
| ----------------------------------------------------------------- | ---------------------------------- |
| `EIBridge.import_action(input_map_action, value_type=BOOL)`       | returns an `EIAction` with no defaults |
| `EIBridge.import_context(input_map_action, name, priority=0)`     | 1:1 mappings, no modifiers/triggers; returns null Ref for unknown actions |

### Enums

```gdscript
enum EIAction.ValueType { BOOL, AXIS1D, AXIS2D, AXIS3D }

# Dispatch trigger events. The enum is bound on EISubsystem, so from
# GDScript use the fully-qualified constant names, e.g.
# EISubsystem.EI_TRIGGER_EVENT_TRIGGERED.
enum EISubsystem.ETriggerEvent {
    EI_TRIGGER_EVENT_NONE       = 0,
    EI_TRIGGER_EVENT_STARTED    = 1,
    EI_TRIGGER_EVENT_TRIGGERED  = 2,
    EI_TRIGGER_EVENT_ONGOING    = 3,
    EI_TRIGGER_EVENT_COMPLETED  = 4,
    EI_TRIGGER_EVENT_CANCELED   = 5,
}
```

---

## Migration from `InputMap` (`EIBridge`)

If you already have a populated `InputMap` and want to start using EI without
rewriting every binding, `EIBridge` is a one-shot read-only importer:

```gdscript
# Bring an InputMap action into EI as an EIAction (no defaults yet).
var ia_jump := EIBridge.import_action("ui_accept", EIAction.VALUE_TYPE_BOOL)

# Or, build an entire IMC from the InputMap action's events.
var ctx := EIBridge.import_context("ui_accept", "Gameplay", priority := 0)
# ctx now has one bare mapping per InputMap event:
#   - event   = the original InputEvent
#   - action  = a new EIAction named "ui_accept"
#   - mods    = []
#   - triggers = []
#   - consumes = true
# Layer your own modifiers / triggers on top via add_mapping() afterwards:
var hold := EITriggerHold.new(); hold.hold_time_threshold = 0.3
ctx.add_mapping(jump_event, ia_jump, [], [hold], false)

Engine.get_singleton("EISubsystem").add_mapping_context(ctx, 0)
```

Caveats (per spec §4.10):

* **Read-only.** `EIBridge` never touches `InputMap` or `Input`.
* **Lossy v1.** Triggers, modifiers, and composite bindings are not imported —
  you add them explicitly afterwards.
* The `priority` argument to `import_context` is currently accepted but not
  stored on the IMC (priority is a registration concern, not an authoring
  one). Pass it to `add_mapping_context()` instead.

---

## Testing

```bash
# Full suite (Godot doctest runner).
./bin/godot.<platform>.editor.x86_64.console.exe --headless --test

# List all test cases (sanity check).
./bin/godot.<platform>.editor.x86_64.console.exe --headless --test --list-test-cases
```

> **Which binary?** Use the non-`dev` binary — `godot.<platform>.editor.<arch>.console.exe`,
> without `.dev.` in the filename. The `.dev.` variants are produced when
> building with `dev_build=yes` and **do not** support `--test` even when
> `tests=yes` is also passed to SCons. So the recommended build for
> development is `dev_build=no tests=yes` (the SCons default), not the
> usual `dev_build=yes` you'd use for normal engine hacking.

EnhancedInput tests are tagged `[EnhancedInput]`. After P6 the suite has 95 EI
test cases across 6 files; the full suite is around 1450 tests.

If you only changed EnhancedInput and want to iterate fast:

```bash
python -m SCons platform=windows target=editor tests=yes -j$(nproc)
./bin/godot.windows.editor.x86_64.console.exe --headless --test
```

Test files live in `modules/enhanced_input/tests/` and follow the `test_*.h`
naming convention. `modules/modules_tests.gen.h` is regenerated by SCons on
every build — don't edit it manually.

---

## Implementation phases

| Phase | Scope                                                  | Status |
| ----- | ------------------------------------------------------ | ------ |
| P1    | Skeleton, `EIValue`, `EISubsystem` stub                | done   |
| P2    | `EIAction` + `EIModifier` base classes                 | done   |
| P3    | 5 concrete modifiers (DeadZone, Negate, Scale, Normalize, SwizzleAxis) | done |
| P4    | 7 concrete triggers (Pressed, Hold, Tap, DoubleTap, Pulse, Chord, Release) | done |
| P5    | `EIMappingContext`, `EIInputEventSampler`, dispatcher, `EIComponent` | done |
| P6    | `EIBridge` — read-only importer from `InputMap`        | done   |
| P7    | Inspector plugin (dropdown for modifier/trigger subclasses, IMC list rendering) | wired   |
| P8    | Polish, examples, project-side demo (e.g. Plinko integration) | **TODO** |

See the file header comments of each `.cpp` for the spec section it implements
and the design decisions it makes.

---

## Spec reference

The full design document is `enhanced-input.md` (v0.2). The module implements:

* §4.1 EIValue
* §4.2 EIAction
* §4.3 EIModifier (+ 5 concrete subclasses)
* §4.4 EITrigger (+ 7 concrete subclasses)
* §4.5 EIMappingContext
* §4.6 EISubsystem (dispatch, state, queries)
* §4.7 EIComponent
* §4.10 EIBridge

§4.8 (built-in .tres resources) and §4.9 (inspector plugin) land with P7.