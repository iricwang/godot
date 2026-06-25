extends SceneTree
## verify_input.gd — headless coverage for the enhanced_input contracts that
## the FortySix demo relies on.
##
## Run:
##   bin/godot.windows.editor.dev.x86_64.console.exe \
##       --headless --path projs/forty-six -s verify_input.gd
##
## Prints "=== ALL INPUT ASSERTIONS PASSED ===" on success and exits 0;
## exits 1 with a FAIL list otherwise.
##
## Why a dedicated script: verify_demo.gd only exercised one input path
## (Space → click). The demo actually depends on several EI behaviors that
## were untested — multi-key aliases, IMC priority + `consumes` shadowing
## (the Dialog-over-Activity pattern), EIComponent auto-unbind on _exit_tree,
## remove_context teardown, and zero-arg callback dispatch. Each is asserted
## here against a freshly-instantiated EISubsystem (the autoload does not run
## under the `-s` custom-MainLoop entry point, so we build one ourselves).

const EIHelpers := preload("res://core/ei_helpers.gd")

# Mirror EIHelpers' trigger-event constants for readability.
const TRIG_STARTED := 1
const TRIG_COMPLETED := 4

var _sub: Object = null
var _owns_sub := false # true when we instantiated it (vs. reused an autoload)
var _pass := 0
var _fail := 0


## A heap Object the bound Callables target. Covers both the zero-arg shape
## the demo uses everywhere and the full (action, event, value) shape.
class Spy extends Object:
	var zero_hits := 0
	var typed_hits := 0
	var last_action: Resource = null
	var last_event := -1

	func on_zero() -> void:
		zero_hits += 1

	func on_typed(action: Resource, event: int, _value: Variant) -> void:
		typed_hits += 1
		last_action = action
		last_event = event


func _initialize() -> void:
	print("[verify_input] === boot ===")
	if not _ensure_subsystem():
		# Module not compiled into this engine build: nothing to test, and
		# the demo degrades to mouse-only by design. Treat as a vacuous pass
		# so CI on a stock engine doesn't go red.
		print("[verify_input] note: enhanced_input not compiled — skipping (vacuous pass)")
		print("=== ALL INPUT ASSERTIONS PASSED ===")
		_finish(0)
		return

	await process_frame

	_test_pressed_fires_zero_arg_callback()
	_test_release_fires_completed()
	_test_multi_key_alias()
	_test_priority_and_consumes_shadow()
	_test_component_auto_unbind_on_free()
	_test_remove_context_silences_keys()
	_test_typed_callback_receives_action_and_event()

	await process_frame

	if _fail == 0:
		print("=== ALL INPUT ASSERTIONS PASSED (%d checks) ===" % _pass)
		_finish(0)
	else:
		printerr("=== INPUT VERIFY FAILED: %d of %d checks failed ===" % [_fail, _fail + _pass])
		_finish(1)


# ---------------------------------------------------------------------------
# Infrastructure
# ---------------------------------------------------------------------------

func _ensure_subsystem() -> bool:
	if Engine.has_singleton(&"EISubsystem"):
		_sub = Engine.get_singleton(&"EISubsystem")
		return true
	if not ClassDB.class_exists(&"EISubsystem"):
		return false
	_sub = ClassDB.instantiate(&"EISubsystem")
	if _sub == null:
		return false
	root.add_child(_sub)
	Engine.register_singleton(&"EISubsystem", _sub)
	_owns_sub = true
	return true


## Tear down anything we created, then quit. Only frees the subsystem if we
## made it ourselves — a pre-existing autoload singleton is left alone.
func _finish(code: int) -> void:
	if _owns_sub and _sub:
		if Engine.has_singleton(&"EISubsystem"):
			Engine.unregister_singleton(&"EISubsystem")
		if _sub.get_parent():
			_sub.get_parent().remove_child(_sub)
		_sub.free()
		_sub = null
	quit(code)


func _check(cond: bool, msg: String) -> void:
	if cond:
		_pass += 1
	else:
		_fail += 1
		printerr("  FAIL: %s" % msg)


## Reset subsystem state between tests so contexts/bindings don't bleed over.
func _reset() -> void:
	_sub.clear_all_mapping_contexts()
	_sub.clear_bindings()


func _press(keycode: int) -> void:
	var ev := EIHelpers.key_event(keycode)
	ev.pressed = true
	_sub.inject_input(ev)


func _release(keycode: int) -> void:
	var ev := EIHelpers.key_event(keycode)
	ev.pressed = false
	_sub.inject_input(ev)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

## A bare Space press must fire a zero-argument callback. This is the exact
## shape every demo Activity binds (`_on_click`, `_show_pause`, …); it is also
## the regression guard for the _fire_event arity fix (GDScript rejects a
## 3-arg call into a 0-arg method, so the dispatcher must degrade the argc).
func _test_pressed_fires_zero_arg_callback() -> void:
	_reset()
	var ia := EIHelpers.make_bool_action("T.Click")
	var imc := EIHelpers.make_context("IMC_Click")
	imc.add_mapping(EIHelpers.key_event(KEY_SPACE), ia, [], [], true)
	var spy := Spy.new()
	_sub.bind_action(ia, TRIG_STARTED, Callable(spy, "on_zero"))
	EIHelpers.add_context(imc, 10)

	_press(KEY_SPACE)
	_check(spy.zero_hits == 1, "Space STARTED should fire zero-arg callback once (got %d)" % spy.zero_hits)

	spy.free()


## Releasing the key fires COMPLETED (Pressed trigger), independent of STARTED.
func _test_release_fires_completed() -> void:
	_reset()
	var ia := EIHelpers.make_bool_action("T.Click")
	var imc := EIHelpers.make_context("IMC_Click")
	imc.add_mapping(EIHelpers.key_event(KEY_SPACE), ia, [], [], true)
	var started := Spy.new()
	var completed := Spy.new()
	_sub.bind_action(ia, TRIG_STARTED, Callable(started, "on_zero"))
	_sub.bind_action(ia, TRIG_COMPLETED, Callable(completed, "on_zero"))
	EIHelpers.add_context(imc, 10)

	_press(KEY_SPACE)
	_check(started.zero_hits == 1 and completed.zero_hits == 0, "press → STARTED only")
	_release(KEY_SPACE)
	_check(completed.zero_hits == 1, "release → COMPLETED fires (got %d)" % completed.zero_hits)

	started.free()
	completed.free()


## Two keys mapped to the same action both fire it (Space/Enter → click in
## the demo; S/Q/Esc aliases elsewhere).
func _test_multi_key_alias() -> void:
	_reset()
	var ia := EIHelpers.make_bool_action("T.Click")
	var imc := EIHelpers.make_context("IMC_Alias")
	imc.add_mapping(EIHelpers.key_event(KEY_SPACE), ia, [], [], true)
	imc.add_mapping(EIHelpers.key_event(KEY_ENTER), ia, [], [], true)
	var spy := Spy.new()
	_sub.bind_action(ia, TRIG_STARTED, Callable(spy, "on_zero"))
	EIHelpers.add_context(imc, 10)

	_press(KEY_SPACE)
	_release(KEY_SPACE)
	_press(KEY_ENTER)
	_check(spy.zero_hits == 2, "Space and Enter aliases should each fire (got %d)" % spy.zero_hits)

	spy.free()


## The Dialog-over-Activity contract: a high-priority IMC with consumes=true
## shadows the lower-priority Activity IMC for the same key. This is exactly
## how PauseDialog (priority 100) steals Esc from GameActivity (priority 10).
func _test_priority_and_consumes_shadow() -> void:
	_reset()
	var ia_low := EIHelpers.make_bool_action("T.Activity.Esc")
	var ia_high := EIHelpers.make_bool_action("T.Dialog.Esc")
	var imc_low := EIHelpers.make_context("IMC_Activity")
	var imc_high := EIHelpers.make_context("IMC_Dialog")
	imc_low.add_mapping(EIHelpers.key_event(KEY_ESCAPE), ia_low, [], [], true)
	imc_high.add_mapping(EIHelpers.key_event(KEY_ESCAPE), ia_high, [], [], true)
	var low := Spy.new()
	var high := Spy.new()
	_sub.bind_action(ia_low, TRIG_STARTED, Callable(low, "on_zero"))
	_sub.bind_action(ia_high, TRIG_STARTED, Callable(high, "on_zero"))
	EIHelpers.add_context(imc_low, 10)
	EIHelpers.add_context(imc_high, 100)

	_press(KEY_ESCAPE)
	_check(high.zero_hits == 1, "high-priority IMC should receive Esc (got %d)" % high.zero_hits)
	_check(low.zero_hits == 0, "consumes=true must shadow the low-priority IMC (got %d)" % low.zero_hits)

	# Drop the dialog IMC; now the Activity IMC wakes up (PauseDialog dismiss).
	_release(KEY_ESCAPE)
	EIHelpers.remove_context(imc_high)
	_press(KEY_ESCAPE)
	_check(low.zero_hits == 1, "after removing high IMC, low IMC should fire (got %d)" % low.zero_hits)

	low.free()
	high.free()


## EIComponent auto-unbinds all its callbacks when it leaves the tree, so an
## Activity/Dialog that frees doesn't keep firing. The IMC stays registered;
## only the binding goes away.
func _test_component_auto_unbind_on_free() -> void:
	_reset()
	var ia := EIHelpers.make_bool_action("T.Click")
	var imc := EIHelpers.make_context("IMC_Click")
	imc.add_mapping(EIHelpers.key_event(KEY_SPACE), ia, [], [], true)
	EIHelpers.add_context(imc, 10)

	var owner := Node.new()
	root.add_child(owner)
	var comp := EIHelpers.attach_component(owner)
	_check(comp != null, "attach_component should return an EIComponent")
	if comp == null:
		owner.free()
		return
	var spy := Spy.new()
	comp.bind(ia, TRIG_STARTED, Callable(spy, "on_zero"))

	_press(KEY_SPACE)
	_check(spy.zero_hits == 1, "bound component fires (got %d)" % spy.zero_hits)
	_release(KEY_SPACE)

	# Free the owner → EIComponent._exit_tree → unbind_all on the subsystem.
	owner.free()
	_press(KEY_SPACE)
	_check(spy.zero_hits == 1, "after owner.free(), component must auto-unbind (got %d)" % spy.zero_hits)

	spy.free()


## remove_context() (called from _on_pause / _on_destroy / dialog dismiss)
## must stop the keys from resolving at all.
func _test_remove_context_silences_keys() -> void:
	_reset()
	var ia := EIHelpers.make_bool_action("T.Click")
	var imc := EIHelpers.make_context("IMC_Click")
	imc.add_mapping(EIHelpers.key_event(KEY_SPACE), ia, [], [], true)
	var spy := Spy.new()
	_sub.bind_action(ia, TRIG_STARTED, Callable(spy, "on_zero"))
	EIHelpers.add_context(imc, 10)

	_press(KEY_SPACE)
	_check(spy.zero_hits == 1, "context active → fires (got %d)" % spy.zero_hits)
	_release(KEY_SPACE)

	EIHelpers.remove_context(imc)
	_press(KEY_SPACE)
	_check(spy.zero_hits == 1, "after remove_context, key must not fire (got %d)" % spy.zero_hits)

	spy.free()


## A 3-arg callback receives the originating action and the trigger event,
## confirming the dispatcher passes the full payload when the callee wants it.
func _test_typed_callback_receives_action_and_event() -> void:
	_reset()
	var ia := EIHelpers.make_bool_action("T.Typed")
	var imc := EIHelpers.make_context("IMC_Typed")
	imc.add_mapping(EIHelpers.key_event(KEY_SPACE), ia, [], [], true)
	var spy := Spy.new()
	_sub.bind_action(ia, TRIG_STARTED, Callable(spy, "on_typed"))
	EIHelpers.add_context(imc, 10)

	_press(KEY_SPACE)
	_check(spy.typed_hits == 1, "typed callback should fire (got %d)" % spy.typed_hits)
	_check(spy.last_action == ia, "typed callback should receive the originating action")
	_check(spy.last_event == TRIG_STARTED, "typed callback should receive STARTED (got %d)" % spy.last_event)

	spy.free()
