extends SceneTree
# build_demo_assets.gd — generate the P10 demo's .tres resources.
#
# Run: godot --headless --path plinko_game -s build_demo_assets.gd
#
# What this writes:
#   res://ia_drop.tres       — IA_Drop  (Bool,  default trigger: Pressed)
#   res://ia_pause.tres      — IA_Pause (Bool,  default trigger: Pressed)
#   res://ia_resume.tres     — IA_Resume(Bool,  default trigger: Pressed)
#   res://imc_menu.tres      — IMC_Menu:  UI accept -> IA_Resume
#                              (Space and Enter both unpause from main menu)
#   res://imc_gameplay.tres  — IMC_Gameplay: Space -> IA_Drop, Escape -> IA_Pause
#   res://imc_paused.tres    — IMC_Paused:   Escape -> IA_Resume (consumes=true)
#
# Why we don't hand-write the .tres: the text format must include the
# right header (`[gd_resource type="EIAction" ... format=3]`) and Godot's
# TypedArray<EIModifier> / TypedArray<EITrigger> serialization is fiddly
# (it expects a typed-array wrapper). Letting ResourceSaver do the
# serialization guarantees the on-disk file matches what the loader
# will produce next reload.

const DIR := "res://"

func _init() -> void:
	print("[build_demo_assets] === generating P10 .tres ===")

	# --- EIAction resources -------------------------------------------
	# ValueType enum: 0=BOOL, 1=AXIS1D, 2=AXIS2D, 3=AXIS3D
	# (per ei_action.h, registered via VARIANT_ENUM_CAST)
	const VAL_BOOL := 0

	_save_action("ia_drop.tres",
			"IA_Drop",
			"Drop a ball on Space (Bool; default Pressed trigger).",
			VAL_BOOL)

	_save_action("ia_pause.tres",
			"IA_Pause",
			"Open pause overlay on Escape from gameplay.",
			VAL_BOOL)

	_save_action("ia_resume.tres",
			"IA_Resume",
			"Close pause overlay on Escape from paused, or Enter/Space from menu.",
			VAL_BOOL)

	# --- EIMappingContext resources -----------------------------------
	# The InputEvents are plain InputEventKey resources — Godot loads
	# them through the engine's default deserializer, no .tres needed.
	var ev_space = _key(KEY_SPACE)
	var ev_enter = _key(KEY_ENTER)
	var ev_escape = _key(KEY_ESCAPE)

	# IMC_Menu: only Resume is bound (Space or Enter resumes).
	# Priority 0 = base layer; gameplay/pushed on top.
	var imc_menu = _new_imc("IMC_Menu", 0)
	imc_menu.add_mapping(ev_space, _load_ia("ia_resume.tres"), [], [], true)
	imc_menu.add_mapping(ev_enter, _load_ia("ia_resume.tres"), [], [], true)
	_save_imc(imc_menu, "imc_menu.tres")

	# IMC_Gameplay: Space -> Drop, Escape -> Pause.
	# Priority 10; consumed events stop fall-through to menu.
	var imc_gameplay = _new_imc("IMC_Gameplay", 10)
	imc_gameplay.add_mapping(ev_space, _load_ia("ia_drop.tres"), [], [], false)
	imc_gameplay.add_mapping(ev_escape, _load_ia("ia_pause.tres"), [], [], true)
	_save_imc(imc_gameplay, "imc_gameplay.tres")

	# IMC_Paused: only Escape -> Resume (so Space doesn't drop a ball
	# while paused). consumes=true so the event doesn't leak through to
	# gameplay underneath. Priority 20 — top of the stack.
	var imc_paused = _new_imc("IMC_Paused", 20)
	imc_paused.add_mapping(ev_escape, _load_ia("ia_resume.tres"), [], [], true)
	_save_imc(imc_paused, "imc_paused.tres")

	print("[build_demo_assets] === done ===")
	quit()


# --- helpers ---------------------------------------------------------

func _save_action(p_path: String, p_name: String, p_desc: String, p_type: int) -> void:
	var ia = ClassDB.instantiate("EIAction")
	ia.value_type = p_type
	ia.description = "[%s] %s" % [p_name, p_desc]
	# Default trigger = EITriggerPressed. The dispatcher only evaluates
	# triggers listed in EIAction.default_triggers (per
	# EISubsystem::_ensure_action_triggers); leaving the array empty
	# means NO events fire for this action. EITriggerPressed gives us
	# the standard "Started on press, Triggered once, Completed on
	# release" lifecycle that maps to OnJump/OnPause/OnResume.
	var pressed = ClassDB.instantiate("EITriggerPressed")
	var trigs: Array = ia.default_triggers
	trigs.append(pressed)
	ia.default_triggers = trigs
	# Tag the resource with its action name for readability in the
	# inspector. EIAction doesn't store a name field itself; we just
	# put it in the description.
	var err = ResourceSaver.save(ia, DIR + p_path)
	assert(err == OK, "save %s failed (err=%d)" % [p_path, err])
	print("[build_demo_assets] wrote %s (type=%d, triggers=%d)" % [p_path, p_type, ia.default_triggers.size()])

func _new_imc(p_name: String, p_priority: int) -> Resource:
	# C++ namespace `ei::EIMappingContext` registers as GDScript class
	# `EIMappingContext` (the namespace is stripped by GDREGISTER_CLASS).
	# We type as Resource to avoid a hard dependency on the class symbol
	# being resolved at script-compile time — the build script runs from
	# `godot -s` and a missing class symbol would hard-fail compilation.
	var imc: Resource = ClassDB.instantiate("EIMappingContext")
	imc.context_name = p_name
	# Priority is set when we register with the subsystem (push), not on
	# the resource itself. Keep the priority hint in the name for
	# readability: "IMC_Paused@20".
	if p_priority != 0:
		imc.context_name = "%s@%d" % [p_name, p_priority]
	return imc

func _save_imc(p_imc: Resource, p_path: String) -> void:
	var err = ResourceSaver.save(p_imc, DIR + p_path)
	assert(err == OK, "save %s failed (err=%d)" % [p_path, err])
	print("[build_demo_assets] wrote %s (mappings=%d)" % [p_path, p_imc.get_mapping_count()])

func _load_ia(p_path: String) -> Resource:
	var res = load(DIR + p_path)
	assert(res != null, "load %s failed" % p_path)
	assert(res.get_class() == "EIAction", "%s is not an EIAction (got %s)" % [p_path, res.get_class()])
	return res

func _key(p_keycode: int) -> InputEventKey:
	var ev = InputEventKey.new()
	ev.physical_keycode = p_keycode
	ev.keycode = p_keycode
	return ev
