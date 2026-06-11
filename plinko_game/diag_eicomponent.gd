extends SceneTree
# diag_eicomponent.gd — direct headless test: does EIComponent.bind()
# forward to the autoload-owned EISubsystem the same way
# ei.bind_action() does?
#
# Both paths should end up calling the same bound Callable when the
# matching (action, event) pair fires. We bind a probe via each path
# and dispatch a Space press in Menu; whichever path lands on the
# dispatching subsystem prints a line.
#
# Run: godot --headless --path plinko_game -s diag_eicomponent.gd

func _init() -> void:
	print("[diag] === boot ===")
	# Wait for the autoload (ei_autoload.gd) to register the singleton.
	# Autoloads' _ready runs after SceneTree._init, so we yield.
	for i in range(5):
		await process_frame
	var ei: Object = null
	if Engine.has_singleton("EISubsystem"):
		ei = Engine.get_singleton("EISubsystem")
	if ei == null:
		print("[diag] EISubsystem singleton still missing after 5 frames — autoload not running?")
		quit()
		return
	print("[diag] ei instance_id=%d" % ei.get_instance_id())

	# Load resources
	var ia_drop = load("res://ia_drop.tres")
	var imc_menu = load("res://imc_menu.tres")
	var imc_gameplay = load("res://imc_gameplay.tres")

	# Push IMC_Menu so Space maps to IA_Resume (we'll observe that path)
	ei.add_mapping_context(imc_menu, 0)
	print("[diag] pushed IMC_Menu, has=%s" % str(ei.has_mapping_context(imc_menu)))

	# --- 1. Direct bind path (known to work) -------------------------
	# Bind to IA_Resume (which is what IMC_Menu's Space mapping targets).
	var ia_resume = load("res://ia_resume.tres")
	var direct_fired := [false]
	ei.bind_action(ia_resume, 1, func(_a, _e, _v): direct_fired[0] = true; print("[diag] DIRECT fired"))
	print("[diag] direct bind done")

	# --- 2. EIComponent bind path (suspected broken) -----------------
	var comp: Object = ClassDB.instantiate("EIComponent")
	# Add to root so it's in the tree
	root.add_child(comp)
	var comp_fired := [false]
	comp.bind(ia_resume, 1, func(_a, _e, _v): comp_fired[0] = true; print("[diag] COMPONENT fired"))
	print("[diag] component bind done, bound_count=%d" % comp.get_bound_count())
	print("[diag] component.is_bound(IA_Resume, STARTED)=%s" % str(comp.is_bound(ia_resume, 1)))

	# --- 3. Drive the dispatch ---------------------------------------
	# The autoload's _input isn't running here (we're not in a scene
	# tree that has the autoload's _input in the path the way it does
	# in plinko_demo). So we call ei.inject_input directly.
	var ev = InputEventKey.new()
	ev.keycode = KEY_SPACE
	ev.physical_keycode = KEY_SPACE
	ev.pressed = true
	print("[diag] about to inject_input")
	ei.inject_input(ev)
	print("[diag] inject_input returned")
	await process_frame

	print("[diag] direct_fired=%s comp_fired=%s" % [str(direct_fired[0]), str(comp_fired[0])])
	if direct_fired[0] and not comp_fired[0]:
		print("[diag] === BUG CONFIRMED: EIComponent.bind does not forward to subsystem ===")
	elif direct_fired[0] and comp_fired[0]:
		print("[diag] === BOTH paths work; earlier failure was a different issue ===")
	elif not direct_fired[0]:
		print("[diag] === UNEXPECTED: direct bind did not fire (subsystem dispatch issue) ===")
	quit()
