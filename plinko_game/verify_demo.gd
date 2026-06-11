extends SceneTree
# verify_demo.gd — headless smoke test for plinko_demo.
#
# Boots the plinko_demo.tscn scene, drives it with synthetic
# Input.parse_input_event calls, and checks that:
#   1. EISubsystem autoload registers correctly.
#   2. IA_Drop's Pressed trigger fires Started → bound Callable runs.
#   3. _push_gameplay() / _pop_top() re-route Space → IA_Drop.
#
# Run: godot --headless --path plinko_game -s verify_demo.gd

const SCENE_PATH := "res://scenes/plinko_demo.tscn"

var _demo_node: Node = null


func _init() -> void:
	print("[verify_demo] === boot ===")
	# Load the scene and instantiate. Add to root so _ready runs in
	# a real SceneTree.
	var packed: PackedScene = load(SCENE_PATH)
	assert(packed != null, "could not load %s" % SCENE_PATH)
	_demo_node = packed.instantiate()
	assert(_demo_node != null, "instantiate failed")
	root.add_child(_demo_node)
	# Wait several frames for the demo's async _ready to complete
	# (it awaits process_frame before doing subsystem work).
	for i in range(10):
		await process_frame

	# --- 1. Sanity: autoload is up + IMCs loaded ----------------------
	var ei: Object = Engine.get_singleton("EISubsystem")
	assert(ei != null, "EISubsystem singleton missing")
	assert(_demo_node._ia_drop != null, "demo failed to load IA_Drop")
	assert(_demo_node._imc_gameplay != null, "demo failed to load IMC_Gameplay")
	assert(ei.has_mapping_context(_demo_node._imc_menu), "Menu IMC not registered after _ready")
	print("[verify_demo] 1. autoload + resources OK (1 IMC active)")

	# --- 2. In Menu mode, Space is mapped to IA_Resume (not IA_Drop).
	# Pressing Space should NOT increment _score_drop.
	var drops_before: int = _demo_node._score_drop
	_press_key(KEY_SPACE)
	await process_frame
	await process_frame
	_release_key(KEY_SPACE)
	await process_frame
	await process_frame
	assert(_demo_node._score_drop == drops_before, "Space in Menu must not trigger IA_Drop")
	print("[verify_demo] 2. Space-in-Menu does NOT trigger IA_Drop OK")

	# --- 3. Switch to Gameplay and press Space → IA_Drop fires once.
	_demo_node._push_gameplay()
	await process_frame
	await process_frame
	var drops_pre: int = _demo_node._score_drop
	_press_key(KEY_SPACE)
	await process_frame
	await process_frame
	_release_key(KEY_SPACE)
	await process_frame
	await process_frame
	assert(_demo_node._score_drop == drops_pre + 1,
			"Space in Gameplay must trigger IA_Drop once (pre=%d post=%d)"
			% [drops_pre, _demo_node._score_drop])
	print("[verify_demo] 3. Space-in-Gameplay triggers IA_Drop OK (drops=%d)" % _demo_node._score_drop)

	# --- 4. Press Esc in Gameplay → IA_Pause pushes IMC_Paused.
	_press_key(KEY_ESCAPE)
	await process_frame
	await process_frame
	_release_key(KEY_ESCAPE)
	await process_frame
	await process_frame
	assert(_demo_node._active_imcs.size() == 2,
			"expected 2 active IMCs (Gameplay+Paused), got %d" % _demo_node._active_imcs.size())
	print("[verify_demo] 4. Esc-in-Gameplay pushes IMC_Paused OK")

	# --- 5. Press Esc in Paused → IA_Resume pops the top.
	_press_key(KEY_ESCAPE)
	await process_frame
	await process_frame
	_release_key(KEY_ESCAPE)
	await process_frame
	await process_frame
	assert(_demo_node._active_imcs.size() == 1,
			"expected 1 active IMC after Esc-in-Paused, got %d" % _demo_node._active_imcs.size())
	print("[verify_demo] 5. Esc-in-Paused pops top OK")

	# --- 6. Print the captured log so we can see it in CI -------------
	var log_label: RichTextLabel = _demo_node.get_node("VBox/LogLabel")
	print("[verify_demo] --- captured log ---")
	for line in log_label.text.split("\n"):
		if line.strip_edges() != "":
			print("  ", line)
	print("[verify_demo] --- end log ---")

	print("[verify_demo] === ALL ASSERTIONS PASSED ===")
	quit()


# --- helpers --------------------------------------------------------

func _press_key(p_keycode: int) -> void:
	var ev := InputEventKey.new()
	ev.keycode = p_keycode
	ev.physical_keycode = p_keycode
	ev.pressed = true
	Input.parse_input_event(ev)

func _release_key(p_keycode: int) -> void:
	var ev := InputEventKey.new()
	ev.keycode = p_keycode
	ev.physical_keycode = p_keycode
	ev.pressed = false
	Input.parse_input_event(ev)
