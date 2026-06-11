# Enhanced Input for Godot 4.7 — Detailed Spec

> Spec id: `enhanced-input`  
> Source proposal: `openspec/changes/enhanced-input/proposal.md`  
> Targets: Godot 4.7+ (GDExtension ABI 4.3+)

This document is the single source of truth for the design of the Enhanced Input GDExtension. It is referenced by all sub-capabilities in the change.

---

## 1. Background and Design Principles

### 1.1 Why a parallel layer instead of replacing Godot's input?

Godot 4.7's `Input` and `InputMap` are battle-tested, document-rich, and integrated with the scene tree's `_input / _unhandled_input` lifecycle. Replacing them would (a) break every existing Godot project, (b) fork the engine, and (c) lose the per-device state tracking and accumulated input that Godot already does well.

The Enhanced Input layer is a **parallel superset**:

- It listens to the same `InputEvent` stream that Godot dispatches via `Viewport::push_input`.
- It maintains its own action state map, independent of `Input.action_states`.
- It exposes its own subscription API; legacy `Input.is_action_pressed` keeps working.
- A `bridge` helper (`EIBridge.import_action` / `EIBridge.import_context`) lets users lift existing InputMap entries into EI without touching them.

#### 1.1.1 Hook model (where EI receives input events)

EI hooks into the scene tree at a single, well-defined point:

- `EISubsystem` is registered as an **autoload Node** so it lives in the root viewport's tree.
- It overrides `Node::_input(Ref<InputEvent>)`. Because the autoload is the first Node in the root viewport, its `_input` is dispatched first by `Viewport::push_input` (after GUI has had a chance to consume, which is what we want: UI buttons eat the click first, then gameplay sees the unhandled one).
- When a mapping has `consumes=true` and fires, EI calls `get_viewport()->set_input_as_handled()`. This stops the event from reaching subsequent nodes' `_input` callbacks. (Godot 4.7 has deprecated `push_unhandled_input()`; we use the modern `set_input_as_handled()` flow exclusively.)
- EI does **not** hook the deprecated `Window.window_input` signal: that fires *before* GUI handling, which would let gameplay intercept events that the UI should consume.

```cpp
void EISubsystem::_input(Ref<InputEvent> p_event) {
    if (is_input_disabled()) return;
    _dispatch_event(p_event);
    // Note: do NOT call set_input_as_handled() unconditionally;
    // only the matching mapping's `consumes` flag decides.
}
```

The autoload registration is a one-line `project.godot` entry:
```ini
[autoload]
EISubsystem="*res://addons/enhanced_input/ei_subsystem.gd"
```
(In a GDExtension, the singleton is registered via `register_types.cpp` calling `Engine::register_singleton("EISubsystem", ...)`.)

### 1.2 Design principles

1. **Strict layering** — Core engine → EI core (state) → EI modifiers/triggers (pure functions on values/states) → User code. Each layer only depends on the one below.
2. **Composability** — Triggers and modifiers are value-typed objects (`Ref<>`) that can be chained; ordering is well-defined.
3. **Determinism** — Every `ETriggerEvent` transition has a defined cause. `Started` happens exactly once per `Started→Triggered` arc. No "phantom" events from lost focus or app unfocus unless explicitly configured.
4. **No global mutable state outside `EISubsystem`** — Modifiers and triggers are pure: given a value, return a value. All per-action / per-frame state lives in `EISubsystem`.
5. **Serializable** — Every resource is `.tres`-serializable so the editor can be the configuration surface.

---

## 2. Module / Repository Layout

```
D:\AI_Temp\Godot\enhanced_input_gdextension\
├── enhanced_input.gdextension          # GDExtension manifest
├── SConstruct                          # SCons build script
├── godot-cpp/                          # godot-cpp submodule (4.7 branch)
├── src/
│   ├── register_types.h
│   ├── register_types.cpp
│   ├── ei_action.h / .cpp
│   ├── ei_value.h / .cpp
│   ├── ei_modifier.h / .cpp
│   ├── ei_trigger.h / .cpp
│   ├── ei_mapping_context.h / .cpp
│   ├── ei_subsystem.h / .cpp
│   ├── ei_component.h / .cpp
│   ├── ei_input_event_sampler.h / .cpp
│   ├── ei_action_state.h / .cpp
│   ├── ei_event_logger.h / .cpp
│   ├── ei_bridge.h / .cpp
│   ├── modifiers/
│   │   ├── ei_modifier_dead_zone.h / .cpp
│   │   ├── ei_modifier_negate.h / .cpp
│   │   ├── ei_modifier_scale.h / .cpp
│   │   ├── ei_modifier_normalize.h / .cpp
│   │   └── ei_modifier_swizzle_axis.h / .cpp
│   └── triggers/
│       ├── ei_trigger_pressed.h / .cpp
│       ├── ei_trigger_hold.h / .cpp
│       ├── ei_trigger_tap.h / .cpp
│       ├── ei_trigger_double_tap.h / .cpp
│       ├── ei_trigger_pulse.h / .cpp
│       ├── ei_trigger_chord.h / .cpp
│       └── ei_trigger_release.h / .cpp
├── editor/
│   ├── ei_inspector_plugin.h / .cpp
│   ├── ei_modifier_picker.h / .cpp
│   └── ei_trigger_picker.h / .cpp
├── demo/                                # optional demo, used by tests too
│   ├── plinko_demo.gd
│   ├── plinko_demo.tscn
│   └── resources/
│       ├── ia_jump.tres
│       ├── ia_move.tres
│       ├── ia_charge.tres
│       ├── imc_menu.tres
│       ├── imc_gameplay.tres
│       └── imc_paused.tres
├── tests/
│   ├── test_ei_value.cpp                # doctest or godot-cpp test
│   ├── test_ei_modifiers.cpp
│   ├── test_ei_triggers.cpp
│   ├── test_ei_subsystem.cpp
│   └── test_ei_bridge.cpp
└── README.md
```

---

## 3. Build Setup

### 3.1 `enhanced_input.gdextension`

```ini
[configuration]
entry_symbol = "enhanced_input_library_init"
compatibility_minimum = "4.3"

[libraries]
windows.debug.x86_64 = "res://bin/libenhanced_input.windows.template_debug.x86_64.dll"
windows.release.x86_64 = "res://bin/libenhanced_input.windows.template_release.x86_64.dll"
linux.debug.x86_64 = "res://bin/libenhanced_input.linux.template_debug.x86_64.so"
macos.debug = "res://bin/libenhanced_input.macos.template_debug.framework"
```

### 3.2 SConstruct (skeleton)

```python
#!/usr/bin/env python
import os
env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp") + Glob("src/modifiers/*.cpp") + Glob("src/triggers/*.cpp")
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "bin/libenhanced_input.{}.{}.framework/libenhanced_input.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "bin/libenhanced_input{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )
Default(library)
```

---

## 4. Class Specifications

### 4.0 Shared Enums (in `core/ei_enums.h`)

A small header holding the enums that cross class boundaries. In godot-cpp 4.x, plain `enum` is required (not `enum class`) so `VARIANT_ENUM_CAST` works.

```cpp
// ei_enums.h
enum ETriggerEvent {
    EI_TRIGGER_EVENT_NONE = 0,
    EI_TRIGGER_EVENT_STARTED,
    EI_TRIGGER_EVENT_TRIGGERED,
    EI_TRIGGER_EVENT_ONGOING,
    EI_TRIGGER_EVENT_COMPLETED,
    EI_TRIGGER_EVENT_CANCELED,
};

// Aggregation priority used by EISubsystem. Higher = wins on conflict.
enum ETriggerEventPriority {
    EI_TRIGGER_PRIORITY_NONE = 0,
    EI_TRIGGER_PRIORITY_CANCELED = 10,
    EI_TRIGGER_PRIORITY_STARTED = 20,
    EI_TRIGGER_PRIORITY_COMPLETED = 30,
    EI_TRIGGER_PRIORITY_ONGOING = 40,
    EI_TRIGGER_PRIORITY_TRIGGERED = 50,
};
```

`EIValue` and `EIAction::ValueType` enum stay in their own headers but follow the same plain-`enum` pattern.

### 4.1 `EIValue` (in `core/ei_value.h`)

A first-class value type for action data. Implemented as a **tagged union** over a `Type` enum and the four payload fields — no `Variant` allocation. Variant serialization is a separate concern (handled by `ei_value_serializer.h` for `.tres` I/O only).

```cpp
// ei_value.h
class EIValue {
public:
    enum Type {
        TYPE_BOOL = 0,
        TYPE_AXIS1D = 1,
        TYPE_AXIS2D = 2,
        TYPE_AXIS3D = 3,
    };

    static EIValue make_bool(bool p_b);
    static EIValue make_axis1d(float p_v);
    static EIValue make_axis2d(const Vector2 &p_v);
    static EIValue make_axis3d(const Vector3 &p_v);

    Type get_type() const;
    bool  get_bool() const;       // valid for TYPE_BOOL
    float get_axis1d() const;     // valid for TYPE_AXIS1D
    Vector2 get_axis2d() const;   // valid for TYPE_AXIS2D
    Vector3 get_axis3d() const;   // valid for TYPE_AXIS3D

    bool is_zero() const;          // value == 0 (typed)
    bool operator==(const EIValue &p_other) const;
    bool operator!=(const EIValue &p_other) const;

    // Promote / demote across types. Fails noisily in debug, returns zero-value in release.
    EIValue with_type_promoted(Type p_target) const;

    // Variant round-trip for .tres serialization (lives in ei_value_serializer.h).
    // NOT used at runtime — every modifier/trigger takes EIValue by value/const ref.
    Variant to_variant() const;
    static EIValue from_variant(const Variant &p_v);

private:
    Type _type = TYPE_BOOL;
    union {
        bool _bool_v;
        float _axis1d_v;
        Vector2 _axis2d_v;
        Vector3 _axis3d_v;
    };
};
```

#### Requirements
- `EIValue` SHALL be a value type, copyable, comparable, hashable. **No heap allocation** in the runtime path.
- `EIValue` SHALL never silently coerce across types. `with_type_promoted` is the only conversion path.
- `get_axis1d()` called on a non-AXIS1D value SHALL return `0.0f` and emit an error message in debug builds (`#ifdef DEBUG_ENABLED`).
- `to_variant` / `from_variant` are for serialization only. They SHALL be implemented in a separate header (`ei_value_serializer.h`) so the hot path stays free of `Variant` overhead.
- `is_zero()` is a typed zero-check: `false` for bool, `==0.0f` for axis1d, `Vector2()==axis2d`, etc. Used by `EITriggerRelease` for transition detection.

#### Scenarios
- Given `EIValue::make_axis2d(Vector2(0, 1))`, `get_axis2d()` returns `Vector2(0, 1)`.
- Given an Axis1D value `0.5f`, `with_type_promoted(TYPE_AXIS2D)` produces an Axis2D value with `Vector2(0.5, 0)`.
- `EIValue::make_axis3d(Vector3(1, 2, 3)).is_zero()` returns `false`.
- Serializing an `EIValue` to `.tres` and reloading it round-trips.

---

### 4.2 `EIAction` (in `core/ei_action.h`)

```cpp
class EIAction : public Resource {
    GDCLASS(EIAction, Resource);

public:
    enum ValueType {
        VALUE_TYPE_BOOL,
        VALUE_TYPE_AXIS1D,
        VALUE_TYPE_AXIS2D,
        VALUE_TYPE_AXIS3D,
    };

    void set_value_type(ValueType p_t);
    ValueType get_value_type() const;

    void set_default_modifiers(const TypedArray<EIModifier> &p_mods);
    TypedArray<EIModifier> get_default_modifiers() const;

    void set_default_triggers(const TypedArray<EITrigger> &p_trigs);
    TypedArray<EITrigger> get_default_triggers() const;

    void set_description(const String &p_desc);
    String get_description() const;

protected:
    static void _bind_methods();

private:
    ValueType _value_type = VALUE_TYPE_BOOL;
    TypedArray<EIModifier> _default_modifiers;
    TypedArray<EITrigger> _default_triggers;
    String _description;
};
```

#### Requirements
- `EIAction` SHALL be a `Resource` so it can be saved as `.tres`.
- `default_modifiers` SHALL be applied AFTER per-mapping modifiers in the IMC binding.
- `default_triggers` SHALL be applied AFTER per-mapping triggers in the IMC binding.
- An `EIAction` with `VALUE_TYPE_BOOL` SHALL be triggerable by single-button events.
- An `EIAction` with `VALUE_TYPE_AXIS1D/2D/3D` SHALL also accept a single `InputEventKey` mapped with `value=1.0` and `value=-1.0` modifiers (W/S → 1.0/-1.0 for Axis1D move).
- A project SHALL reference the same `EIAction` resource from multiple `EIMappingContext`s and the `EISubsystem` SHALL dedupe state.

#### Scenarios
- `EIAction "IA_Jump"` (Bool) bound in `IMC_Gameplay` on Space key — pressing Space triggers `Started→Triggered` immediately (with default `EITriggerPressed`).
- `EIAction "IA_Move"` (Axis2D) bound in two IMCs with W/A/S/D keys + Negate modifier on S/D — pressing W+D yields `(1, 0)`, W+A+D yields `(-1+1, 0)=(0, 0)`.

---

### 4.3 `EIModifier` and Subclasses (in `core/ei_modifier.h`)

Abstract base:

```cpp
class EIModifier : public Resource {
    GDCLASS(EIModifier, Resource);

public:
    // Pure function. Returns transformed value. Implementations must be deterministic.
    virtual EIValue modify_value(const EIValue &p_input) const = 0;

    virtual String get_modifier_name() const = 0;  // for inspector

protected:
    static void _bind_methods();
};
```

#### Built-in modifiers

| Class | Behavior | Configurable |
|---|---|---|
| `EIModifierDeadZone` | 1D: `abs(x)<deadzone ? 0 : sign(x)*(abs(x)-deadzone)/(1-deadzone)`; 2D: radial deadzone | `min_deadzone`, `max_deadzone` (for dual-stick radials) |
| `EIModifierNegate` | 1D: `x = -x`; 2D: per-axis bool | `x`, `y`, `z` bools |
| `EIModifierScale` | Multiplies by `Vector3(scale_x, scale_y, scale_z)` | scalar per axis |
| `EIModifierNormalize` | 2D/3D: re-scale to length 1 if non-zero | min length to trigger |
| `EIModifierSwizzleAxis` | Rearranges axes: e.g. `(x, y, z) → (y, z, x)` | enum order |

#### Requirements
- Every `EIModifier` SHALL be a pure function (no side effects, no state).
- `modify_value` SHALL be safe to call from any thread.
- Modifiers that change value type (none in the v1 set) are not allowed.

#### Scenarios
- `EIModifierDeadZone(0.2)` on `Axis1D(0.15)` → `0.0`; on `Axis1D(0.5)` → `(0.5-0.2)/(1-0.2) = 0.375`.
- `EIModifierNegate { x=false, y=true }` on `Axis2D(1, 1)` → `Axis2D(1, -1)`.
- `EIModifierNormalize` on `Axis2D(3, 4)` → `Axis2D(0.6, 0.8)`.

---

### 4.4 `EITrigger` and Subclasses (in `core/ei_trigger.h`)

Trigger state machine per action:

```cpp
class EITrigger : public Resource {
    GDCLASS(EITrigger, Resource);

public:
    enum TriggerState {
        STATE_NONE,
        STATE_ONGOING,
        STATE_TRIGGERED,
    };

    enum UpdateResult {
        RESULT_NONE,
        RESULT_STARTED,
        RESULT_TRIGGERED,
        RESULT_ONGOING,
        RESULT_COMPLETED,
        RESULT_CANCELED,
    };

    // Per-action state owned by EISubsystem, passed in by value.
    struct TriggerRuntimeState {
        TriggerState state = STATE_NONE;
        double hold_start_time = -1.0;
        double last_fire_time = -1.0;
        int consecutive_tap_count = 0;
        double last_tap_time = -1.0;
    };

    // Called every event arrival and every _process tick (with p_event_valid=false on tick).
    virtual UpdateResult update_state(
        TriggerRuntimeState &p_runtime,
        const EIValue &p_value,
        double p_delta_t,
        bool p_event_valid,
        bool p_pressed
    ) const = 0;

    virtual String get_trigger_name() const = 0;
};
```

#### Built-in triggers

| Class | Semantics | Config |
|---|---|---|
| `EITriggerPressed` | On `event_valid && p_pressed` fire `Started→Triggered→Completed` | — |
| `EITriggerHold` | After held `hold_time_threshold` seconds, fire `Started→Triggered`, then `Ongoing` each tick, `Completed` on release | `hold_time_threshold` |
| `EITriggerTap` | Fire `Triggered` on press, expect release within `tap_release_time`; if held too long, `Canceled` | `tap_release_time` |
| `EITriggerDoubleTap` | Two presses within `double_tap_time`, fire `Triggered` on the 2nd | `double_tap_time` |
| `EITriggerPulse` | Fire `Triggered` every `pulse_interval` seconds while held | `pulse_interval` |
| `EITriggerChord` | Fire when ALL listed `EIAction` (chord actions) are currently in `Triggered` state | list of `EIAction` |
| `EITriggerRelease` | Fire `Triggered` on the frame the value transitions from non-zero to zero | — |

#### Trigger event lifecycle (UE-aligned)

```
            ┌──── event arrives, value > 0
            │
   None ── Started ── Triggered ── Ongoing ── Ongoing ── Completed (on release)
            │              │
            │              └──── (no Ongoing supported) ─ Completed
            └──── (cancel before Triggered) ─ Canceled
```

#### Requirements
- `EITrigger` subclasses SHALL be deterministic given `(runtime_state, value, dt, event_valid, pressed)`.
- `TriggerRuntimeState` is mutated in place; it is **not** stored in the trigger resource itself (so the same trigger can be reused across actions).
- The `EISubsystem` SHALL aggregate multiple triggers per action. Aggregation rule: if ANY trigger returns a non-`NONE` event, the highest-priority event wins. Priority: `Triggered > Ongoing > Started > Completed > Canceled > None`. If both `Started` and `Triggered` fire on the same event, the listener receives `Triggered`.
- Multiple triggers SHALL be AND-combined when evaluating the "should fire" question: an action only becomes active if ALL triggers permit it. (UE Enhanced Input uses AND semantics.)

#### Scenarios
- `EITriggerHold(0.3)` on Space held for 0.5s: `Started` at 0s, `Triggered` at 0.3s, `Ongoing` at 0.4s, `Ongoing` at 0.5s, `Completed` on release.
- `EITriggerTap(0.2)` on Space held for 0.3s: `Started` → 0.1s in state `Ongoing` → 0.2s `Canceled` (no Triggered ever fired).
- `EITriggerChord([IA_Dash, IA_Attack])`: only fires when both are currently Triggered.

---

### 4.5 `EIMappingContext` (in `core/ei_mapping_context.h`)

```cpp
class EIMappingContext : public Resource {
    GDCLASS(EIMappingContext, Resource);

public:
    class Mapping {
        Ref<InputEvent> event;             // e.g. InputEventKey, InputEventMouseButton, InputEventJoypadButton
        Ref<EIAction> action;
        TypedArray<EIModifier> modifiers;  // applied BEFORE action.default_modifiers
        TypedArray<EITrigger> triggers;    // applied BEFORE action.default_triggers
        bool consumes = true;              // fire set_input_as_handled() after dispatch
    };

    void add_mapping(const Ref<InputEvent> &p_event, const Ref<EIAction> &p_action,
                     const TypedArray<EIModifier> &p_mods,
                     const TypedArray<EITrigger> &p_trigs,
                     bool p_consumes);
    void remove_mapping(const Ref<InputEvent> &p_event, const Ref<EIAction> &p_action);
    void clear_mappings();

    TypedArray<Dictionary> get_mappings() const;  // serialized for editor
    int get_mapping_count() const;

    String get_context_name() const;
    void set_context_name(const String &p_name);

protected:
    static void _bind_methods();

private:
    String _context_name;
    Vector<Mapping> _mappings;
};
```

#### Requirements
- `EIMappingContext` SHALL be serializable to `.tres` as a `PackedStringArray` of `Mapping` dictionaries.
- A mapping SHALL support any `InputEvent` subtype (key, mouse button, joypad button, joypad axis, mouse motion).
- For 1D / 2D / 3D actions bound to keys, a single key SHALL be allowed to map to `+1` or `-1` value via `EIModifierNegate`.
- A mapping SHALL have a `consumes` flag: when true, the dispatcher calls `Viewport::set_input_as_handled()` after firing the action, preventing legacy `_input` from seeing it.

---

### 4.6 `EISubsystem` (in `core/ei_subsystem.h`)

```cpp
class EISubsystem : public Node {
    GDCLASS(EISubsystem, Node);

public:
    static EISubsystem *get_singleton();

    void add_mapping_context(const Ref<EIMappingContext> &p_ctx, int p_priority);
    void remove_mapping_context(const Ref<EIMappingContext> &p_ctx);
    void clear_all_mapping_contexts();
    bool has_mapping_context(const Ref<EIMappingContext> &p_ctx) const;

    // Bind a Callable to a TriggerEvent of an action.
    void bind_action(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
    void unbind_action(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
    void clear_bindings();

    // Programmatic injection (for AI / replay / tests).
    void inject_input(const Ref<InputEvent> &p_event);

    // State query.
    EIValue get_action_value(const Ref<EIAction> &p_action) const;
    bool is_action_active(const Ref<EIAction> &p_action) const;
    ETriggerEvent get_action_trigger_event(const Ref<EIAction> &p_action) const;  // last fired event

    void set_log_level(int p_level);  // 0=silent, 1=event, 2=verbose

protected:
    static void _bind_methods();

    // Lifecycle (see §1.1.1 for hook model).
    void _input(Ref<InputEvent> p_event) override;
    void _process(double p_delta) override;
    void _ready() override;

private:
    struct ContextEntry { Ref<EIMappingContext> ctx; int priority; int order; };
    Vector<ContextEntry> _contexts;
    int _context_seq = 0;

    struct ComponentBinding { ObjectID component; Callable callable; };
    HashMap<ObjectID, HashMap<Ref<EIAction>, HashMap<ETriggerEvent, Vector<ComponentBinding>>>> _bindings;

    // Per-action runtime state, allocated lazily. See A6 review fix for field rationale.
    struct ActionRuntime {
        EIValue current_value;                              // post-modifiers, post-evaluation
        EIValue previous_value;                             // last frame's value, for transition detection (Release trigger)
        EIValue raw_value;                                  // pre-modifiers (sampler output)
        bool is_held = false;                               // current_value != zero
        uint64_t held_since_frame = 0;                      // first frame value became non-zero
        ETriggerEvent last_event = EI_TRIGGER_EVENT_NONE;
        ETriggerEvent previous_event = EI_TRIGGER_EVENT_NONE;
        uint64_t last_event_frame = 0;
        Vector<Ref<EITrigger>> triggers;                    // union of per-mapping + default
        HashMap<Ref<EITrigger>, EITrigger::TriggerRuntimeState> trigger_states;
    };
    HashMap<Ref<EIAction>, ActionRuntime> _action_runtimes;

    // Internals
    void _dispatch_event(Ref<InputEvent> p_event);
    void _process_triggers(double p_delta);
    void _fire_event(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const EIValue &p_value);
    void _on_application_focus_changed(bool p_focused);  // hooked to MainLoop's signal
};
```

#### Requirements
- `EISubsystem` SHALL be a singleton (autoload) with one instance per `SceneTree`.
- Context priority SHALL be the primary ordering: higher priority first. Ties broken by insertion order (`_context_seq`).
- For each incoming `InputEvent`, the dispatcher SHALL:
  1. Iterate contexts in priority order.
  2. For each context, iterate mappings in declaration order.
  3. For the first mapping whose `event.matches(event)` returns true, compute the value (apply mapping modifiers, then action default modifiers) and update the runtime.
  4. Stop processing further mappings in lower-priority contexts only if `consumes=true`. Lower-priority mappings SHALL still receive the same event when `consumes=false` (useful for layered UIs).
- `_process` SHALL tick all active triggers with `p_event_valid=false` so that `EITriggerHold` and `EITriggerPulse` advance even between key events.
- `bind_action` SHALL be reentrant: calling it twice with the same action/event/callable SHALL be idempotent.
- When a context is removed at runtime, all actions that only had mappings in that context SHALL be released (no `Completed` event; their state is simply cleared).

#### Trigger event aggregation

When multiple triggers on the same action fire on the same frame, the system SHALL:
1. Collect all non-`NONE` results.
2. If any result is `Triggered`, the listener receives `Triggered` (the strongest "fired" event).
3. Else if any result is `Ongoing`, the listener receives `Ongoing`.
4. Else if any result is `Started`, the listener receives `Started`.
5. Else if any result is `Completed`, the listener receives `Completed`.
6. Else if any result is `Canceled`, the listener receives `Canceled`.

The list above is the priority order. Multiple triggers with AND semantics: the action only "fires" if the AND of "should fire" evaluations is true.

#### Scenarios
- `IMC_Gameplay` priority 0; `IMC_Menu` priority 10. When Menu is active, its mappings are matched first. If `IMC_Menu` has no mapping for Space, fall through to `IMC_Gameplay`.
- `IMC_Menu.consumes=false` for IA_Click, `IMC_Gameplay.consumes=true` for IA_Jump — when both active and a click event arrives, both layers receive it.

---

### 4.7 `EIComponent` (in `core/ei_component.h`)

```cpp
class EIComponent : public Node {
    GDCLASS(EIComponent, Node);

public:
    void bind(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
    void unbind(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event, const Callable &p_callable);
    void unbind_all();

    // Editor / debug introspection.
    bool is_bound(const Ref<EIAction> &p_action, ETriggerEvent p_trigger_event) const;
    int get_bound_count() const;

protected:
    static void _bind_methods();
    void _ready() override;
    void _exit_tree() override;

private:
    struct Subscription { Ref<EIAction> action; ETriggerEvent event; Callable callable; };
    Vector<Subscription> _subscriptions;
};
```

> **Design note (A2 fix):** A `template <typename T> bind_method(...)` overload was originally considered but dropped — C++ method-pointer binding is awkward in GDExtension, and the standard usage is GDScript-side: `component.bind(ia_jump, EI.TRIGGER_EVENT_TRIGGERED, _on_jump)`. The plain `Callable` overload is sufficient for both C++ and GDScript callers.

#### Requirements
- `EIComponent` SHALL auto-register with `EISubsystem::get_singleton()` on `_ready`.
- `EIComponent` SHALL auto-unregister on `_exit_tree`, removing all its bindings.
- If a bound `Callable` becomes invalid (target freed), the system SHALL silently skip it without spamming errors. Optional debug log.
- `is_bound` SHALL return `true` only if a subscription matches both the action and the trigger event exactly.

---

### 4.8 `EIInputEventSampler` (internal helper)

Bridges Godot's `InputEvent` stream into the per-action `EIValue`. Owns the "key → Axis1D(±1.0)", "joypad axis → Axis1D(0..1)" mappings.

```cpp
class EIInputEventSampler {
public:
    // For Axis1D / 2D / 3D actions, the sampler reads the event's `strength` or axis value.
    // For key events, `InputEventKey` is treated as Axis1D(1.0) on press, Axis1D(0.0) on release.
    // For joypad motion, the axis value is read directly.
    static EIValue sample(const Ref<InputEvent> &p_event, EIAction::ValueType p_value_type);
};
```

---

### 4.9 Editor Inspector (in `editor/ei_inspector_plugin.h`)

```cpp
class EIInspectorPlugin : public EditorInspectorPlugin {
    GDCLASS(EIInspectorPlugin, EditorInspectorPlugin);

public:
    bool _can_handle(Object *p_object) const override;
    void _parse_begin(Object *p_object) override;
    void _parse_property(Object *p_object, Variant::Type p_type, const String &p_name, PropertyHint p_hint, const String &p_hint_string, BitField<PropertyUsageFlags> p_usage) override;
};
```

#### Requirements
- For `EIAction`: expose a "Add Modifier" / "Add Trigger" button that opens a `EditorResourcePicker` filtered to `EIModifier` / `EITrigger` subclasses.
- For `EIMappingContext`: render mappings as an editable `EditorInspector` table.

---

### 4.10 `EIBridge` (in `core/ei_bridge.h`)

```cpp
class EIBridge : public RefCounted {
public:
    // Convert a Godot InputMap action into an EIAction + EIMappingContext.
    // Each InputEvent under the action becomes one Mapping.
    // No modifiers / triggers added — bare-bones 1:1 conversion.
    static Ref<EIAction> import_action(const StringName &p_input_map_action, EIAction::ValueType p_type = EIAction::VALUE_TYPE_BOOL);
    static Ref<EIMappingContext> import_context(const StringName &p_input_map_action, const String &p_context_name, int p_priority = 0);
};
```

#### Requirements
- `EIBridge` SHALL NOT modify `InputMap` or `Input`. It is a read-only importer.
- Conversion SHALL be lossy in this v1: triggers, modifiers, composite bindings are NOT imported — they require explicit EI authoring.

---

## 5. Event Flow (End-to-End)

```
DisplayServer window
  └─ Window._window_input(event)
      └─ Viewport.push_input(event)                  [Godot existing]
          ├─ _gui_input_event(event)                 [Godot existing: UI consumes clicks first]
          └─ Node._input(event)  ◀── EISubsystem hooks here (autoload)
              └─ EISubsystem._dispatch_event(event)
                  1. Resolve active contexts sorted by (priority DESC, order ASC).
                  2. For each ctx in order:
                     a. For each mapping in declaration order:
                        i.   if mapping.event.matches(event) == false: continue
                        ii.  value = EIInputEventSampler.sample(event, action.value_type)
                        iii. for m in mapping.modifiers: value = m.modify_value(value)
                        iv.  for m in action.default_modifiers: value = m.modify_value(value)
                        v.   update ActionRuntime.{previous_value, current_value, raw_value, is_held}
                        vi.  for t in (mapping.triggers + action.default_triggers):
                             state = runtime.trigger_states[t]
                             evt = t.update_state(state, value, dt, p_event_valid=true, p_pressed=action.is_held)
                             runtime.trigger_states[t] = state
                        vii. aggregate TriggerEvents across triggers → single ETriggerEvent (priority-based)
                        viii. _fire_event(action, evt, value)
                        ix.   if mapping.consumes: get_viewport()->set_input_as_handled()  ← stops other Nodes
                  3. return
              └─ Subsequent Nodes' _input (if not set_input_as_handled)  [Godot existing]
```

> **Note:** Godot 4.7 deprecated `push_unhandled_input()` / `_unhandled_input()` (see `viewport.cpp:3558`). EI uses the modern `set_input_as_handled()` flow exclusively and never relies on the deprecated path.

Per-frame `_process` (drives timer-based triggers like Hold, Pulse):
```
EISubsystem._process(delta):
  for action in _action_runtimes:
    for t in action.triggers:
      state = action.trigger_states[t]
      evt = t.update_state(state, action.current_value, delta,
                           p_event_valid=false, p_pressed=action.is_held)
      if evt != NONE: aggregate + _fire_event
```

---

## 6. Edge Cases & Guarantees

### 6.0 Behavior contracts

- **App unfocus**: `EISubsystem` SHALL `cancel` all in-progress triggers when the main window loses focus. The signal `application_focus_changed` is on the `MainLoop` singleton — in `_ready` we connect `Engine::get_main_loop()->connect("application_focus_changed", callable_mp(this, &EISubsystem::_on_application_focus_changed))`. On focus loss, all `ActionRuntime.is_held == true` entries SHALL fire `CANCELED` and have `current_value` reset to zero. No phantom `Completed` events.

### 6.1 Non-goals (out of scope for v1)

The following are **explicitly NOT in v1** to keep scope tight. They are listed so future contributors know what hooks to add without breaking the v1 contract.

- **Mobile touch / gesture input** — `InputEventScreenTouch / InputEventScreenDrag / InputEventGesture` are not routed through `EIInputEventSampler`. The sampler API is left extensible for P-v2.
- **Input recording / replay** — no event log file format in v1. `EISubsystem::inject_input()` covers programmatic injection.
- **Networked input sync** — no per-peer wire format. Each peer's local input is its own.
- **Multiplayer / split-screen** — `EISubsystem` is a single-process singleton. Split-screen on one machine is feasible (per-viewport IMCs) but unverified.
- **Localized event names** — all enum / property names are English; no translation strings.
- **Editor visual graph for triggers** — triggers are configured via property inspector, not a node graph.
- **`UInputModifier` `ConsumesInput` semantics** — UE's "consume input on fire" flag is not in v1. The `mapping.consumes` flag covers the common case.
- **Custom `InputEvent` subclasses from GDExtension** — EI only accepts engine-built-in `InputEvent` types. Extending `InputEvent` from GDExtension is technically possible but unverified.
- **Profile-guided trigger optimization** — no hot-path profiling tools. Plain sequential iteration.

### 6.2 Dispatch rules (binding order & conflict resolution)

- **Modifier chain order**: per-mapping modifiers (in declaration order) → action default modifiers. Documented in code.
- **Trigger aggregation**: strongest event wins (priority order: `TRIGGERED > ONGOING > STARTED > COMPLETED > CANCELED > NONE`). Multiple triggers on the same action are AND-combined for "should fire" evaluation.
- **Same event, multiple mappings, same context**: first matching mapping wins; the rest are skipped in that context for that event.
- **Same event, multiple contexts**: each context gets a chance, in priority order, unless a higher-priority context's `consumes=true` mapping already fired.

### 6.3 Serialization guarantees

- **Modifier identity in `.tres`**: `EIModifier` subclasses register with `ClassDB`; the inspector can instantiate them by class name.
- **Resource circular refs**: disallowed; detected at load time (e.g. an `EITriggerChord` referencing its own action transitively).
- **`.tres` forward compat**: when a new field is added to `EIAction` / `EIMappingContext` / etc., old `.tres` files load with default values for the missing field (Godot's default behavior).

---

## 7. Test Strategy

### 7.1 Unit (C++ doctest)

- `test_ei_value.cpp` — type conversion, promotion.
- `test_ei_modifiers.cpp` — every modifier, golden values, edge cases (0, max, deadzone boundary).
- `test_ei_triggers.cpp` — Pressed / Hold / Tap / DoubleTap / Pulse / Chord / Release, simulate event timeline with mock `dt`.
- `test_ei_subsystem.cpp` — context priority ordering, consume flag, dedupe bindings.
- `test_ei_bridge.cpp` — round-trip an InputMap action through `EIBridge`.

### 7.2 Integration (Godot `tests/`)

- `test_ei_integration.gd` — headless test: register IMC with 2 mappings, fire synthetic InputEventKey, assert callbacks fired.
- Plinko demo scene used as smoke test: load `plinko_game/`, verify no console errors and `IA_Jump` triggers `OnJump()` on Space.

### 7.3 Performance

- 1000 events/frame × 10 IMC × 50 mappings = 50,000 mapping checks/frame. Target < 0.5ms.
- Profiling: `Profiler::add_frame_sampler` callback. Report any regression vs raw Godot.

---

## 8. Open Decisions (to revisit at P1)

1. **Action "type" vs modifier "promotion"**: Should `EIAction{VALUE_TYPE_BOOL}` bound to a 1D value auto-promote, or fail? Recommendation: fail, require explicit Axis1D action. Implementer can override.
2. **Modifier order**: Per-mapping THEN per-action. Confirm in user feedback.
3. **Trigger AND vs OR across multiple triggers on one action**: UE uses AND. Confirm.
4. **Triggers consuming from action state**: After `EITriggerTap` fires `Triggered`, should the action state be consumed (e.g. buffer cleared) like UE? Recommendation: NO in v1, may add later. (UE's `ConsumesInput` on modifier/trigger.)
5. **Mobile touch**: not in v1 scope. Hook point reserved in `EIInputEventSampler` for future.

---

## 9. File-by-File Implementation Order (P1-P11)

| P | Task | Key files | Verification |
|---|---|---|---|
| **P0** | Confirm decisions from §8 | — | User sign-off |
| **P1** | Skeleton: build godot-cpp, SConstruct, register_types, EIValue | `SConstruct`, `src/register_types.{h,cpp}`, `src/ei_value.{h,cpp}` | `scons` builds `libenhanced_input.dll`. `EISubsystem` autoload loads with empty singleton. |
| **P2** | EIAction, EIModifier base | `src/ei_action.{h,cpp}`, `src/ei_modifier.{h,cpp}` | `.tres` of `EIAction` loads, `value_type` and modifiers editable. |
| **P3** | 5 modifiers: DeadZone, Negate, Scale, Normalize, SwizzleAxis | `src/modifiers/*.{h,cpp}` | `test_ei_modifiers.cpp` passes. |
| **P4** | EITrigger base + 5 core triggers: Pressed, Hold, Tap, DoubleTap, Release | `src/ei_trigger.{h,cpp}`, `src/triggers/{pressed,hold,tap,double_tap,release}.{h,cpp}` | `test_ei_triggers.cpp` passes. |
| **P5** | EIMappingContext, EISubsystem (autoload), EIInputEventSampler | `src/ei_mapping_context.{h,cpp}`, `src/ei_subsystem.{h,cpp}`, `src/ei_input_event_sampler.{h,cpp}` | Headless test: add 2 IMC, fire 1 event, verify priority order. |
| **P6** | EIComponent, bind/unbind lifecycle | `src/ei_component.{h,cpp}` | Component on Node: bind action → callback fires on Space. |
| **P7** | 2 more triggers: Pulse, Chord. 5-event aggregation | `src/triggers/{pulse,chord}.{h,cpp}` | `test_ei_triggers.cpp` extended. |
| **P8** | Editor Inspector | `editor/*.{h,cpp}` | In editor, `EIAction` resource shows Add Modifier / Add Trigger buttons. |
| **P9** | EIBridge | `src/ei_bridge.{h,cpp}` | `import_context("ui_accept")` produces a context with the same events. |
| **P10** | Plinko demo | `plinko_game/scenes/plinko_demo.gd`, `plinko_game/scenes/plinko_demo.tscn`, 3 IMC, 3 IA | Demo loads, Space triggers `OnJump` in Gameplay, ESC triggers `OnPause`. |
| **P11** | Docs + performance profile | `README.md`, sample `.tres` | Profiling report. |

---

## 10. Glossary

| Term | Definition |
|---|---|
| **Action** | Logical input unit (`EIAction`), decoupled from any key/button |
| **Mapping Context (IMC)** | Resource containing key→action mappings; stackable with priority |
| **Modifier** | Pure function transforming a value (e.g. deadzone, negate) |
| **Trigger** | Stateful predicate determining *when* a TriggerEvent fires (Hold, Tap, etc.) |
| **TriggerEvent** | One of Started / Triggered / Ongoing / Completed / Canceled, fired on a callback |
| **Subsystem** | Autoload singleton managing global state, the entry point for the engine's input stream |
| **Component** | Per-character/per-Node binding surface, where the user code calls `bind_action(...)` |

---

## 11. References

- UE Enhanced Input plugin source: `Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/`
- UE Enhanced Input design talk: "Enhanced Input - What. Why. How." (Epic, 2022)
- Godot 4 `core/input/input.cpp` (this repo)
- Godot 4 `core/input/input_map.cpp` (this repo)
- Godot 4 `scene/main/viewport.cpp` (this repo)
- godot-cpp: https://github.com/godotengine/godot-cpp (4.7 branch)
