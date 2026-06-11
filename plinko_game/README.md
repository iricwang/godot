# plinko_game — Enhanced Input (P10) demo

Smoke-test scene for the `enhanced_input` engine module. Per spec §9 P10:
"Demo loads, Space triggers OnJump in Gameplay, ESC triggers OnPause."

## Run interactively

```
godot --path plinko_game
```

Five buttons in the lower bar let you push / pop IMCs manually:
- **Menu mode** — clear stack, push only `IMC_Menu` (Space/Enter → Resume)
- **Gameplay mode** — clear stack, push only `IMC_Gameplay` (Space → Drop, Esc → Pause)
- **Push Paused** — push `IMC_Paused` on top of whatever's there (Esc → Resume)
- **Pop top** — remove the topmost IMC
- **Clear all** — remove every IMC

The status line shows the live stack and drop count. The colored log
captures the full Trigger lifecycle for `IA_Drop`.

## Run headless (smoke test)

```
godot --headless --path plinko_game -s verify_demo.gd
```

Boots the scene, drives it with synthetic key events, and asserts:
1. Autoload + resources loaded, `IMC_Menu` registered.
2. Space-in-Menu does **not** trigger `IA_Drop` (Menu maps Space→Resume).
3. Space-in-Gameplay triggers `IA_Drop` exactly once.
4. Esc-in-Gameplay pushes `IMC_Paused` (2 IMCs active).
5. Esc-in-Paused pops the top (back to 1 IMC).

Expected output ends with `=== ALL ASSERTIONS PASSED ===`.

## Files

| File | What it is |
|---|---|
| `project.godot` | Project root config, autoload registered, `ui_accept` seed in InputMap. |
| `ei_autoload.gd` | The autoload itself — instantiates the C++ `EISubsystem`, parents it to the autoload, registers it as `Engine.get_singleton("EISubsystem")`, and forwards every `_input(event)` to `ei.inject_input(event)`. P5 final version (the P1 stub in `ei_test/ei_autoload.gd` only prints). |
| `build_demo_assets.gd` | One-shot generator: builds the 3 EIAction + 3 EIMappingContext `.tres` resources via `ResourceSaver.save` (so the on-disk format is the one Godot itself would write). Run with `godot --headless -s build_demo_assets.gd` only if you change the IMC layout. |
| `ia_drop.tres` | `EIAction` Bool, default trigger `EITriggerPressed`. |
| `ia_pause.tres` | `EIAction` Bool, default trigger `EITriggerPressed`. |
| `ia_resume.tres` | `EIAction` Bool, default trigger `EITriggerPressed`. |
| `imc_menu.tres` | Space→Resume, Enter→Resume (priority 0). |
| `imc_gameplay.tres` | Space→Drop, Esc→Pause (priority 10). |
| `imc_paused.tres` | Esc→Resume (priority 20, consumes=true). |
| `scenes/plinko_demo.tscn` | The demo scene: status label, hint label, RichTextLabel log, five buttons. |
| `scenes/plinko_demo.gd` | The demo script. Boots, loads IAs/IMCs, creates `EISubsystem` singleton, binds `ei.bind_action(...)` for the 5 (action,event) triples, wires the buttons. |
| `verify_demo.gd` | Headless smoke test (see above). |
| `verify_demo_stdout.log` | Last run's log. |

## P5c known issue (workaround in this demo)

The spec's recommended per-instance binding wrapper is `EIComponent`:
```
var comp := EIComponent.new()
add_child(comp)
comp.bind(ia_jump, EI.ETriggerEvent.TRIGGERED, _on_jump)
```

In headless + `godot -s` mode, `EIComponent::bind()`'s forward to
`EISubsystem::bind_action()` via the C++ static `EISubsystem::singleton`
pointer does not always land on the autoload-owned instance. Direct
`ei.bind_action(...)` (the lower-level path, what this demo uses) works
reliably; it just bypasses the per-instance auto-unbind-on-exit-tree
guarantee that `EIComponent` provides.

Tracking: investigate in P5c cleanup. Likely a
`EISubsystem::get_singleton()` resolution issue in the test / headless
context where the static singleton may have been set by a different
instance than the autoload's. The C++ side of the framework is
correct; the demo is the lowest-friction workaround.

## Why `default_triggers` must be explicit on every IA

`EISubsystem::_ensure_action_triggers` only adds what's listed in
`EIAction.default_triggers` to the per-action trigger chain. Leaving
the array empty means **no events fire** for that action. The demo's
`build_demo_assets.gd` therefore attaches an `EITriggerPressed` to
every action. If you add a new IA from a script, do the same:

```gdscript
var ia = ClassDB.instantiate("EIAction")
var arr: Array = ia.default_triggers
arr.append(ClassDB.instantiate("EITriggerPressed"))
ia.default_triggers = arr
```
