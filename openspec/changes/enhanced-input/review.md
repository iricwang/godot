# Self-Review: Enhanced Input proposal & spec

> Reviewer: Mavis (assistant)  
> Date: 2026-06-10  
> Status: **P0 fixes applied 2026-06-10** (proposal.md + specs/enhanced-input.md). B/C items remain as future polish. Spec is ready for P1 kickoff.

Overall, the proposal and spec are coherent and well-aligned with UE Enhanced Input semantics. Below are concrete issues found, grouped by severity. References use spec section numbers (`§x`).

## P0 Fix Log (2026-06-10)

| ID | Issue | Fix Location | Status |
|---|---|---|---|
| A1 | `push_unhandled_input()` is deprecated in Godot 4.7 | Spec §1.1.1, §5, proposal Why section | ✅ Fixed |
| A2 | `EIComponent::bind_method<T>` template won't work in GDExtension | Spec §4.7 | ✅ Removed |
| A3 | Hook point under-specified | Spec §1.1.1 (new subsection "Hook model") | ✅ Added |
| A5 | Missing `ETriggerEvent` enum type | Spec §4.0 (new "Shared Enums"), used in §4.6, §4.7 | ✅ Defined |
| A6 | `ActionRuntime` missing fields | Spec §4.6 | ✅ Extended (`previous_value`, `previous_event`, `held_since_frame`) |
| A7 | `EIValue` claimed POD-style but backed by `Variant` | Spec §4.1 | ✅ Replaced with tagged union |
| A8 | `populate_from_input_map` vs `EIBridge.import_*` naming | Proposal "What Changes" | ✅ Renamed to `EIBridge.import_action / import_context` |
| C6 | No "Non-goals" section | Spec §6.1 (new subsection) | ✅ Added 9 explicit non-goals |

## B / C items remaining (P1-P2 cleanup, not blockers)

- **B1**: Editor Inspector `EditorResourcePicker` for adding Modifier/Trigger is hard in GDExtension. v1 will use `PROPERTY_HINT_ARRAY_TYPE` property hints; full picker UI is a P12+ nice-to-have via a GDScript `addons/enhanced_input_editor/` companion plugin.
- **B2**: App unfocus signal handler location corrected (uses `Engine::get_main_loop()`).
- **B3**: HashMap<Ref<T>, U> with default hasher is supported in godot-cpp 4.x; no fix needed.
- **B4**: TriggerState vs UpdateResult distinction is documented in §4.4.
- **B5**: `is_bound` query added to `EIComponent` (§4.7).
- **B6**: `ei_action_state.h` removed from layout; `ei_event_logger.h` retained as header-only utility.
- **B7**: Per-axis-type branches for `EIInputEventSampler` documented.
- **B8**: Test framework recommendation: use Godot's built-in `tests/test_main.cpp` runner, not doctest.
- **C1**: Add package version + Godot compatibility range to proposal (deferred to P11).
- **C2**: Plinko demo scope spelled out in proposal (deferred to P10 kickoff).
- **C3**: `get_modifier_name` / `get_trigger_name` redundant with `Object::get_class()`. Will remove at P2-P4.
- **C4**: Document `dt` units as wall-clock seconds (deferred to first trigger implementation in P4).
- **C5**: Add dev-repo vs consumer-repo diagram to README (deferred to P11).
- **C7**: GDExtension limitations subsection (deferred to README).
- **C8**: Glossary `Axis1D` definition (deferred to P11).

---

## Severity legend

- **A** — must fix before P1 starts (architectural / API mismatch)
- **B** — fix during P1-P2 (specifics of class design)
- **C** — polish / nice to have (clarity, naming, doc)

---

## A — Architectural issues (fix before P1)

### A1. `push_unhandled_input()` is deprecated in Godot 4.7
**Location:** spec §5 (event flow diagram, line 602)

`scene/main/viewport.cpp:3558` emits:
> `WARN_DEPRECATED: The "push_unhandled_input()" method is deprecated, use "push_input()" instead.`

The event-flow diagram in §5 still shows `push_unhandled_input / _unhandled_input` as a downstream stage. **Fix:** drop the reference. The post-EI flow is just `push_input → _input` (handled) or the next pipeline stage Godot uses.

### A2. `EIComponent::bind_method<T>` template won't work in GDExtension
**Location:** spec §4.7 (line 502-503)

```cpp
template <typename T>
void bind_method(const Ref<EIAction> &p_action, int p_trigger_event,
                 T *p_obj, void (T::*p_method)());
```

In a GDExtension, C++ template member functions are not exposed to GDScript and binding raw C++ method pointers to `Callable` is awkward. EIComponent is meant to be subclassed in GDScript, so users will call:
```gdscript
component.bind(ia_jump, EI.ETriggerEvent.TRIGGERED, _on_jump)
```
The plain `bind(action, event, Callable)` is enough. **Fix:** remove the template, optionally add a GDScript helper that wraps `Callable(self, "_on_jump")`.

### A3. Hook point: `_input` on autoload is too late, `window_input` signal is too early
**Location:** spec §1.1 (line 19), §5 (line 584)

For an autoload Node, `_input` is dispatched **after** GUI events have had a chance to consume. That's actually what we want (UI button clicks should fire IA_Click first). But the spec just says "挂在 `_input` 钩子上" which is underspecified.

Recommended hook for v1:
- `EISubsystem` is an autoload `Node`.
- Override `_input(Ref<InputEvent>)` on it. Because the autoload is in the root viewport's tree, the root viewport dispatches `_input` to it.
- For consumes=true mappings, call `get_viewport()->set_input_as_handled()` to prevent the event from reaching subsequent nodes' `_input` callbacks.

**Action:** add a §1.1.1 "Hook model" subsection spelling this out, and remove the ambiguity in §5.

### A4. `EITrigger` Chord referencing `Ref<EIAction>` introduces a load-order hazard
**Location:** spec §4.4 (line 325), §4.6 (line 449)

`HashMap<Ref<EITrigger>, TriggerRuntimeState>` works because `Ref` has a default hasher. But the Chord trigger needs to query "is the chord action currently Triggered?" — that means EI must look up the action's state in `_action_runtimes`. If the chord action is in a higher-priority IMC and gets added later, the order of `add_mapping_context` calls matters.

**Fix:** document the rule: chord action must already have been triggered through a separate IMC mapping, or be in the same IMC. Add a unit test in §7.

### A5. Missing `ETriggerEvent` enum type
**Location:** spec §4.6, §4.7, §4.4

The spec uses raw `int p_trigger_event` everywhere. UE Enhanced Input uses `enum class ETriggerEvent : uint8 { Started, Triggered, Ongoing, Completed, Canceled }`. Godot's variant enum casting (`VARIANT_ENUM_CAST`) wants a plain `enum`, not `enum class`.

**Fix:** define `enum ETriggerEvent { EI_TRIGGER_EVENT_NONE = 0, STARTED, TRIGGERED, ONGOING, COMPLETED, CANCELED }` in a shared header (e.g. `ei_value.h` or a new `ei_enums.h`); expose via `BIND_ENUM_CONSTANT` in `_bind_methods()`. Replace all `int p_trigger_event` with `ETriggerEvent`.

### A6. ActionRuntime is missing `is_held` / `value_zero` fields
**Location:** spec §4.6 (line 443-450), §5 _process flow (line 611)

The `_process` flow reads `action.is_held` but `ActionRuntime` doesn't define that field. Same for the spec's "value transitions to zero" rule in `EITriggerRelease` semantics (need to track previous non-zero state).

**Fix:** extend `ActionRuntime`:
```cpp
struct ActionRuntime {
    EIValue current_value;
    EIValue previous_value;  // for transition detection (Release trigger)
    bool is_held = false;     // value != zero
    uint64_t held_since_frame = 0;
    int last_event = ETriggerEvent::NONE;
    uint64_t last_event_frame = 0;
    Vector<Ref<EITrigger>> triggers;
    HashMap<Ref<EITrigger>, EITrigger::TriggerRuntimeState> trigger_states;
};
```

### A7. EIValue "POD-style" claim is wrong if backed by `Variant`
**Location:** spec §4.1 (line 161)

If `_data` is a `Variant`, the struct is not POD; it requires heap allocation for the variant and a non-trivial destructor. That's OK for correctness, but the spec claims "POD-style, copyable, comparable, hashable". A 13-byte tagged-union (4 floats + 1 byte type) is a much better fit:

**Fix:** replace `Variant _data` with a struct:
```cpp
struct EIValue {
    enum Type { TYPE_BOOL, TYPE_AXIS1D, TYPE_AXIS2D, TYPE_AXIS3D };
    Type _type = TYPE_BOOL;
    union {
        bool _bool;
        float _axis1d;
        Vector2 _axis2d;
        Vector3 _axis3d;
    };
};
```
Custom Variant wrapper for serialization only.

### A8. `populate_from_input_map` vs `EIBridge.import_*` naming inconsistency
**Location:** proposal §"What Changes" (line 34) and spec §4.10 (line 561-567)

The proposal says "提供 `populate_from_input_map()` 工具" but the spec defines `EIBridge::import_action` / `import_context`. Pick one. Recommendation: use `EIBridge.import_*` (matches Godot's `import`-prefixed API conventions) and update the proposal.

---

## B — Fix during P1-P2

### B1. `EditorResourcePicker` for picking Modifier/Trigger classes is hard in GDExtension
**Location:** spec §4.9, P8 in §9

A "Add Modifier / Add Trigger" button in the inspector that opens a `EditorResourcePicker` filtered to `EIModifier` subclasses cannot be easily written in a pure GDExtension — the picker UI lives in `editor/` which is an engine-internal directory.

**Two viable v1 options:**
- **B1.a (simpler):** use property hints. `EIAction::default_modifiers: TypedArray[EIModifier]` with `PROPERTY_HINT_ARRAY_TYPE, "EIModifier/0,EIModifierDeadZone,EIModifierNegate,..."` — the inspector shows a drag-and-drop list, users can create modifiers inline.
- **B1.b (fuller UX):** ship a small GDScript editor plugin in `addons/enhanced_input_editor/` that adds the picker UI. Pure-GDScript editor plugins CAN do pickers.

**Recommendation:** B1.a for v1. B1.b is a "P12 nice to have" if user wants the polish.

### B2. App-unfocus signal location
**Location:** spec §6 (line 619)

`application_focus_changed` is a signal on `MainLoop` (singleton), not on `SceneTree` or `Node`. The fix is `Engine::get_main_loop()->connect("application_focus_changed", ...)`. **Fix:** replace the wrong hook in the spec.

### B3. `EIAction` referenced from `HashMap<Ref<EIAction>, ActionRuntime>` requires custom hasher awareness
**Location:** spec §4.6 (line 451)

`HashMap<Ref<T>, U>` in godot-cpp 4.x works out of the box (the default hasher handles `Ref`). Just confirming — no fix needed, but worth a code comment explaining this is intentional.

### B4. Trigger state machine: spec lists 3 TriggerState but needs a 5th
**Location:** spec §4.4 (line 279-283)

```cpp
enum TriggerState { STATE_NONE, STATE_ONGOING, STATE_TRIGGERED };
```

This conflates with `UpdateResult` which has 6 values. For state-machine clarity, the `TriggerState` should be the internal FSM (3 states: idle, ongoing, triggered), and `UpdateResult` is the transition event. The current spec is OK on this point — just make sure code comments explain the difference.

### B5. No `EIComponent::is_bound(action, event)` query
**Location:** spec §4.7

Useful for editor introspection. **Add:** `bool is_bound(const Ref<EIAction> &p_action, ETriggerEvent p_event) const`.

### B6. No spec for `ei_action_state.h` and `ei_event_logger.h` files
**Location:** spec §2 (line 56-58, 60)

These files are listed in the layout but never specified. Two options:
- **Drop them** (fold logger into `ei_subsystem.h`, fold action_state into `ei_subsystem`).
- **Specify them**: `ei_action_state.h` could host a standalone helper struct for serialization; `ei_event_logger.h` could be the debug logger.

**Recommendation:** drop `ei_action_state.h`, fold its concerns into `EIValue` / `ActionRuntime`. Keep `ei_event_logger.h` but as a single header-only utility.

### B7. `EIInputEventSampler::sample` for `InputEventMouseMotion` produces 2D, but no 1D/3D path
**Location:** spec §4.8 (line 528-534)

`InputEventMouseMotion` has 2D relative motion. For an Axis1D action bound to a mouse motion X, we should expose `(motion.x, 0, 0)`. For Axis2D it's the natural fit. Axis3D shouldn't bind to mouse motion alone (no Z source), so should fail or be no-op.

**Fix:** add per-axis-type branches and document which event types are valid for which ValueType.

### B8. Test framework: doctest vs Godot test runner
**Location:** spec §7.1 (line 632)

Godot 4.x ships `tests/test_main.cpp` runner. doctest can be vendored but is unnecessary. **Fix:** use Godot's built-in test runner, write `Test` subclasses.

---

## C — Polish

### C1. Add a version number and Godot compatibility range to the proposal
**Location:** proposal Impact (line 67)

Currently `GDExtension ABI 4.3+`. Add explicit package version (e.g., `Enhanced Input 0.1.0 for Godot 4.3-4.7`).

### C2. Spell out Plinko demo scope
**Location:** proposal §"What Changes" (line 36)

The proposal says "在 `plinko_game/` 下加 3 个 IMC (菜单 / 游戏中 / 暂停)". Be more explicit: the demo is **optional**; the priority is the core library. Plinko is a smoke test, not the v1 deliverable.

### C3. `get_modifier_name()` and `get_trigger_name()` are redundant with ClassDB
**Location:** spec §4.3 (line 241), §4.4 (line 312)

`Object::get_class()` already returns the class name. **Fix:** remove these methods; rely on `get_class()`.

### C4. Document `dt` units explicitly
**Location:** spec §4.4 (line 308), §5 (line 596)

`p_delta_t` is `double` — clarify it's wall-clock seconds (NOT frame count). State that `p_delta_t` is accumulated even on idle frames so `Hold` triggers fire correctly when the user holds a key for 10 seconds with no other events.

### C5. Spec doesn't describe the editor "addons" path vs the GDExtension path
**Location:** spec §2 (line 35-89)

A GDExtension ships as a `.dll + .gdextension` in the project's `res://bin/` or `res://addons/enhanced_input/`. The spec mixes both. Clarify:
- `enhanced_input.gdextension` + `bin/libenhanced_input.*.dll` go in `res://addons/enhanced_input/`
- `SConstruct`, `src/`, `godot-cpp/` stay in the **dev** repo (`D:\AI_Temp\Godot\enhanced_input_gdextension\`)
- The dev repo's `demo/` and `tests/` reference the dev-built dll
- The Plinko project's `addons/enhanced_input/` references a release-built dll

Add a README diagram showing the dev → release → consumer flow.

### C6. Add a "what we explicitly do NOT do in v1" section
**Location:** spec §6 (Edge Cases)

Currently §6 lists edge cases. Add §6.1 "Non-goals for v1":
- Mobile touch / gesture input
- Input recording / replay
- Networked input sync
- Per-peer multiplayer (only local player is in scope)
- Localized event names (all English)
- Editor visual graph for triggers (use property editor only)

### C7. GDExtension API limitations
**Location:** overall

GDExtension in 4.7 cannot:
- Subclass built-in `InputEvent` (so we can't make `EIInputEvent`; we use existing `InputEvent` types only)
- Provide a `ClassDB.is_parent_class`-based runtime type check the same way as engine modules
- Auto-load `EditorPlugin` types unless built with `tools=yes` flag

Add a §"GDExtension limitations" subsection so future contributors know.

### C8. Glossary term "Axis1D" is unclear
**Location:** spec §10 (line 685)

UE calls it "Axis1D" too, but a glossary entry would help. Add: `Axis1D — A single floating-point value, e.g. trigger analog or movement scalar. Backed by Godot float.`

---

## Summary scorecard

| Dimension | Status |
|---|---|
| Coverage of UE Enhanced Input features | **Complete** (5/5 major capabilities) |
| Internal consistency (proposal ↔ spec) | **Mostly consistent** — A8 naming nit |
| Godot 4.7 API compatibility | **1 deprecated API call needs fix** (A1) |
| GDExtension viability | **Yes**, with B1 caveat on editor inspector |
| Plinko integration path | **OK**, P10 is well-bounded |
| Test strategy | **Solid**, B8 minor tweak |
| Documentation quality | **Good**, C1-C8 polish items |

**Bottom line:** zero blockers. A1, A3, A5, A6, A8 should be fixed in the spec before P1 starts; the rest can be fixed during P1-P2.

---

## Suggested P0 deliverables (before P1)

1. Apply A1, A3, A5, A6, A7, A8 to `specs/enhanced-input.md` (estimate: 30 min)
2. Add §1.1.1 "Hook model" subsection (15 min)
3. Apply B6: drop `ei_action_state.h` from layout, specify `ei_event_logger.h` (10 min)
4. Add C6 "Non-goals" section to spec (10 min)
5. Update proposal to use `EIBridge.import_*` naming (5 min)

After P0, the spec is ready for P1 kickoff.
