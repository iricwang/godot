# FortySix — game_framework Demo

A complete click-counter game (target: 46 clicks) that exercises every
subsystem of the `game_framework` Godot module:

| Subsystem | Where in the demo |
|---|---|
| `Application` + `ActivityManager` + `ServiceRegistry` | `entry.gd` — boots the host, registers two services + four activities + two dialogs, fires the first activity via `FLAG_LOAD_SYNC`. |
| `Activity` lifecycle (`_on_create` / `_on_start` / `_on_resume` / `_on_pause` / `_on_destroy` / `_on_back_pressed`) | Every file under `activities/`. `GameActivity._on_pause` halts the timer; `_on_resume` restarts it. |
| `Dialog` (modal overlay) | `dialogs/pause_dialog.{gd,tscn}` opened from `GameActivity`; `dialogs/confirm_quit_dialog.{gd,tscn}` opened from `MainMenuActivity`. |
| `Toast` (FIFO + custom) | Milestone toasts at 10/20/30/40 in `GameActivity._on_click`, plus settings/Game-Over notifications. |
| `Intent` flags — `FLAG_NEW_CLEAR` / `FLAG_SINGLE_TOP` / `FLAG_NO_HISTORY` / `FLAG_LOAD_SYNC` | `MainMenu→Game` uses `FLAG_NEW_CLEAR`, `MainMenu→Settings` uses `FLAG_SINGLE_TOP`, `Game→GameOver` uses `FLAG_NO_HISTORY`, boot uses `FLAG_LOAD_SYNC`. |
| `Intent.extras` as a result channel | `GameActivity` passes `{score, target, new_record}` to `GameOverActivity`. |
| `ContextProxy` state machine (PENDING → LOADING → READY) | `verify_demo.gd` asserts state transitions explicitly; in-game the async load happens automatically. |
| `ResourceManager.load_resource_async` | `GameActivity._on_create` kicks off a background load just to exercise the polling path. |
| MVVM — `ViewModel`, `ObservableProperty`, `BindingEngine` | One VM per Activity in `view_models/`; bound via `Context.bind_property(...)` in each Activity's `_bind()`. Settings uses `begin/end_bulk_update()` for batched widget signals. |
| `Transition` (FADE / SLIDE_* / SCALE) | Set in code in each Activity / Dialog's `_on_create`. |
| `ServiceRegistry` cross-Activity state sharing | `GameStateService` holds best time / difficulty / volume; Settings writes, Game/MainMenu read. |
| Dialog → owner callback pattern | `PauseDialog` calls `get_lifecycle_owner().on_pause_resume()` etc. — no signals to wire up. |
| Auto owner-cleanup | Milestone toasts owned by `GameActivity` are auto-cancelled if you back out before they show. |

## Running

```
cd <repo root>
bin/godot.windows.editor.dev.x86_64.console.exe --path projs/forty-six
```

For the headless smoke (no window, exits with 0 on green):

```
bin/godot.windows.editor.dev.x86_64.console.exe --headless --path projs/forty-six -s verify_demo.gd
```

Successful smoke ends with:
```
=== ALL ASSERTIONS PASSED ===
```

For the input-specific coverage (enhanced_input contracts: Pressed triggers,
multi-key aliases, IMC priority + `consumes` shadowing, EIComponent
auto-unbind, zero-arg callback dispatch):

```
bin/godot.windows.editor.dev.x86_64.console.exe --headless --path projs/forty-six -s verify_input.gd
```

Successful input run ends with:
```
=== ALL INPUT ASSERTIONS PASSED ===
```

## Layout

```
projs/forty-six/
  project.godot              # main_scene = entry.tscn
  entry.tscn / entry.gd      # boots Application; registers services + activities/dialogs
  core/
    base_view_model.gd       # GDScript wrapper around C++ ViewModel for `self.foo = x` syntax
  services/
    game_state_service.gd    # best_time / total_runs / difficulty / volume
    audio_service.gd         # stub that prints sfx calls
  view_models/
    main_menu_vm.gd
    game_vm.gd
    game_over_vm.gd
    settings_vm.gd
  activities/
    main_menu_activity.{tscn,gd}     # Title + Start / Settings / Quit
    game_activity.{tscn,gd}          # Click target → 46
    game_over_activity.{tscn,gd}     # Score summary, new-record banner
    settings_activity.{tscn,gd}      # Difficulty + Volume
  dialogs/
    pause_dialog.{tscn,gd}            # Resume / Restart / Quit-to-Menu
    confirm_quit_dialog.{tscn,gd}     # Yes / No, dispatches SceneTree.quit
  verify_demo.gd             # Headless smoke walking through the whole flow
  verify_input.gd            # Headless enhanced_input contract coverage
```

## Notes / gotchas worth remembering

1. **Don't name your button callbacks after Activity lifecycle virtuals.**
   Activity has `_on_create / _on_start / _on_resume / _on_pause / _on_stop /
   _on_destroy / _on_back_pressed / _on_new_intent / _on_setup_standalone`.
   `_on_start` is the worst trap — if you `button.pressed.connect(_on_start)`,
   `dispatch_start()` will fire your button handler on every activity start.
   The demo names button handlers `_on_<x>_pressed` to avoid this.

2. **`Activity._on_pause` / `_on_resume` are app-lifecycle, not "user paused".**
   The framework fires `_on_pause` when a new activity is pushed on top, NOT
   when the user opens a pause dialog. Pause-from-dialog is purely cosmetic
   (we toggle `_running` ourselves in `GameActivity._show_pause`).

3. **Dialog dismiss talks back via `get_lifecycle_owner().call("...")`.**
   `PauseDialog` doesn't know about `GameActivity` — it just calls
   `on_pause_resume / on_pause_restart / on_pause_quit_to_menu` on whichever
   owner opened it. Keeps the dialog reusable.

4. **`FLAG_LOAD_SYNC` for the boot and any "instant" navigation.**
   Without it, the first activity goes through the async polling path, so
   `current_activity_proxy()` is `LOADING` for one frame.

5. **`begin_bulk_update()` / `end_bulk_update()` on the ViewModel** when you
   update three or more properties in a row — emits one `value_changed`
   signal at the end instead of three. The Game's `bump()` and Settings'
   `sync_from()` show the pattern.
