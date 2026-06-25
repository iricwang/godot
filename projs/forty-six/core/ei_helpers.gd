extends RefCounted
## EIHelpers — small static-method wrapper around the enhanced_input module
## that keeps the per-Activity boilerplate readable.
##
## Usage:
##   var ia_confirm = EIHelpers.make_bool_action("IA_Confirm")
##   var imc = EIHelpers.make_context("IMC_MainMenu")
##   imc.add_mapping(EIHelpers.key_event(KEY_ENTER), ia_confirm, [], [], true)
##   var comp = EIHelpers.attach_component(self)
##   comp.bind(ia_confirm, EIHelpers.TRIG_STARTED, _on_start_pressed)
##   EIHelpers.add_context(imc, 10)
##
## Symmetric teardown:
##   EIHelpers.remove_context(imc)        # in _on_pause / _on_dismiss
##
## All methods are tolerant of a missing EISubsystem — if the module isn't
## compiled in, builders return null / no-ops so the UI keeps working.

const TRIG_NONE := 0
const TRIG_STARTED := 1   # pressed (rising edge)
const TRIG_TRIGGERED := 2 # pulse (Tap / Hold reached)
const TRIG_ONGOING := 3   # mid-hold
const TRIG_COMPLETED := 4 # released cleanly
const TRIG_CANCELED := 5  # interrupted

const VAL_BOOL := 0
const VAL_AXIS1D := 1
const VAL_AXIS2D := 2


static func subsystem() -> Object:
	if Engine.has_singleton(&"EISubsystem"):
		return Engine.get_singleton(&"EISubsystem")
	return null


static func available() -> bool:
	return subsystem() != null


static func make_bool_action(description: String) -> Resource:
	if not ClassDB.class_exists(&"EIAction"):
		return null
	var a: Resource = ClassDB.instantiate(&"EIAction")
	a.value_type = VAL_BOOL
	a.description = description
	var t: Array = a.default_triggers
	var pressed = ClassDB.instantiate(&"EITriggerPressed")
	if pressed:
		t.append(pressed)
	a.default_triggers = t
	return a


static func make_context(name: String) -> Resource:
	if not ClassDB.class_exists(&"EIMappingContext"):
		return null
	var c: Resource = ClassDB.instantiate(&"EIMappingContext")
	c.context_name = name
	return c


## Builds an InputEventKey carrying the given keycode in BOTH the logical and
## physical fields — matches the gameplay convention used in gf_test so the
## dispatcher recognises the key regardless of which sampler path runs first.
static func key_event(keycode: int) -> InputEventKey:
	var ev := InputEventKey.new()
	ev.physical_keycode = keycode
	ev.keycode = keycode
	return ev


## Attaches a fresh EIComponent as a child of `owner` (Activity / Dialog).
## The Component auto-unbinds its callbacks on _exit_tree (spec §4.7).
static func attach_component(owner: Node) -> Node:
	if not ClassDB.class_exists(&"EIComponent"):
		return null
	var comp: Node = ClassDB.instantiate(&"EIComponent")
	comp.name = "EIComponent"
	owner.add_child(comp)
	return comp


static func add_context(imc: Resource, priority: int = 10) -> void:
	var ei := subsystem()
	if ei and imc:
		ei.add_mapping_context(imc, priority)


static func remove_context(imc: Resource) -> void:
	var ei := subsystem()
	if ei and imc:
		ei.remove_mapping_context(imc)
