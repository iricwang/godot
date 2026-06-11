extends Control
# plinko_demo.gd — P10 smoke test scene for the enhanced_input module.
#
# Per spec §9 P10: "Demo loads, Space triggers OnJump in Gameplay,
# ESC triggers OnPause."
#
# What this scene demonstrates:
#   * Three EIMappingContexts (Menu / Gameplay / Paused) pushed and
#     popped in priority order.
#   * Per-action TriggerEvent subscriptions on an EIComponent (the
#     spec-recommended per-instance binding registry). The component
#     auto-unbinds on _exit_tree.
#   * All five TriggerEvent lifecycle phases exercised: Started for
#     one-shot "rising edge" semantic (drop / pause / resume),
#     Triggered for key-repeat, Completed for release.
#   * EIBridge.import_action() to import the project's existing Godot
#     InputMap entry "ui_accept" (per spec §4.10 — exercises the bridge
#     end-to-end, even if the demo does not bind it).
#   * The fallback story: when no IMC is active, the EI dispatch loop is
#     a no-op (the existing Godot Input singleton still works).
#
# Run interactively:
#   godot --path plinko_game
# Run headless to verify boot + resource round-trip:
#   godot --headless --path plinko_game -s verify_demo.gd
#
# Note on GDScript class names: C++ namespace `ei::EIAction` registers as
# GDScript class `EIAction` (the namespace is stripped by GDREGISTER_CLASS).
# Same for EIMappingContext / EIComponent / EIBridge.

const IA_DROP_PATH := "res://ia_drop.tres"
const IA_PAUSE_PATH := "res://ia_pause.tres"
const IA_RESUME_PATH := "res://ia_resume.tres"
const IMC_MENU_PATH := "res://imc_menu.tres"
const IMC_GAMEPLAY_PATH := "res://imc_gameplay.tres"
const IMC_PAUSED_PATH := "res://imc_paused.tres"

# TriggerEvent enum (per ei_enums.h; 0..5). We use the raw int values so
# the script doesn't need a hard dependency on `ei::ETriggerEvent`
# resolving at compile time.
const TRIG_EVENT_STARTED := 1
const TRIG_EVENT_TRIGGERED := 2
const TRIG_EVENT_ONGOING := 3
const TRIG_EVENT_COMPLETED := 4
const TRIG_EVENT_CANCELED := 5

# Stack-style IMC tracking. Pushed in order, popped from the top. The
# subsystem itself only sees a flat list; we keep this Vector to know
# which IMC to pop when the user wants to leave a mode.
var _active_imcs: Array[Resource] = []

# Preloaded action refs (used for the bind triple-key).
var _ia_drop: Resource
var _ia_pause: Resource
var _ia_resume: Resource

# Preloaded IMC refs.
var _imc_menu: Resource
var _imc_gameplay: Resource
var _imc_paused: Resource

# Drop counter (rising-edge count, surfaced in the status label).
var _score_drop: int = 0

# UI references (filled in _ready).
@onready var _status_label: Label = $VBox/StatusLabel
@onready var _log_label: RichTextLabel = $VBox/LogLabel
@onready var _push_menu_btn: Button = $VBox/Buttons/PushMenuBtn
@onready var _push_gameplay_btn: Button = $VBox/Buttons/PushGameplayBtn
@onready var _push_paused_btn: Button = $VBox/Buttons/PushPausedBtn
@onready var _pop_btn: Button = $VBox/Buttons/PopBtn
@onready var _clear_btn: Button = $VBox/Buttons/ClearBtn


func _ready() -> void:
	# --- 1. Load resources --------------------------------------------
	_ia_drop = load(IA_DROP_PATH)
	_ia_pause = load(IA_PAUSE_PATH)
	_ia_resume = load(IA_RESUME_PATH)
	_imc_menu = load(IMC_MENU_PATH)
	_imc_gameplay = load(IMC_GAMEPLAY_PATH)
	_imc_paused = load(IMC_PAUSED_PATH)

	# --- 2. Resolve EISubsystem --------------------------------------
	# The autoload (ei_autoload.gd) registers the singleton. Wait one
	# frame in case scene load races the autoload.
	await get_tree().process_frame
	var ei: Object = Engine.get_singleton("EISubsystem")
	if ei == null:
		_log("[color=red]EISubsystem singleton missing. Is the autoload enabled?[/color]")
		_status_label.text = "Status: EISubsystem MISSING"
		return
	_status_label.text = "Status: EISubsystem OK"
	# Verbose logging helps the demo's user see the IMC dispatch.
	# 0=silent, 1=event, 2=verbose (per ei_subsystem.h).
	ei.log_level = 2

	# EIBridge demo: import the project's ui_accept (which we set in
	# project.godot) into an EI EIAction. We don't bind it to anything
	# (the demo's own IMC_Menu covers Space/Enter resume), but the
	# printout confirms the bridge wires correctly.
	if ClassDB.class_exists("EIBridge"):
		var bridge: Object = ClassDB.instantiate("EIBridge")
		var imported_ia = bridge.import_action("ui_accept", -1) # -1 = keep detected type
		if imported_ia != null:
			_log("[color=cyan][EIBridge] imported ui_accept → %s (type=%d)[/color]"
					% [imported_ia.get_class(), imported_ia.value_type])
		else:
			_log("[color=yellow][EIBridge] ui_accept not found in InputMap (skip)[/color]")
	else:
		_log("[color=yellow][EIBridge] class not registered (P9 not built?)[/color]")

	# --- 3. Create EIComponent + bind trigger events -----------------
	# EIComponent is the spec-recommended per-instance binding registry.
	# It forwards each `bind(action, event, callable)` to the
	# EISubsystem singleton, and auto-unbinds all of them on _exit_tree
	# (so the demo doesn't leak subscriptions if the scene is freed
	# mid-pause).
	_component = ClassDB.instantiate("EIComponent")
	_component.name = "EIComponent"
	add_child(_component)

	# Per EITriggerPressed semantics (and spec §4.4):
	#   * STARTED  fires on the rising edge of the press event.
	#   * TRIGGERED fires on subsequent presses WHILE still held (i.e.
	#     OS key-repeat — useful for charging meters / autofire).
	#   * COMPLETED fires on the release edge.
	# For a one-shot "OnJump" semantic we bind the actual drop to
	# STARTED. We also log TRIGGERED and COMPLETED so the user can
	# observe the full lifecycle in the on-screen log.
	_component.bind(_ia_drop, TRIG_EVENT_STARTED, _on_drop)
	_component.bind(_ia_drop, TRIG_EVENT_TRIGGERED, _on_drop_repeat)
	_component.bind(_ia_drop, TRIG_EVENT_COMPLETED, _on_drop_released)

	# Pause: only STARTED matters (we want the rising edge — pressing
	# Esc opens the pause overlay once, not repeatedly).
	_component.bind(_ia_pause, TRIG_EVENT_STARTED, _on_pause)

	# Resume: pops the Paused IMC on the rising edge.
	_component.bind(_ia_resume, TRIG_EVENT_STARTED, _on_resume)

	# --- 4. UI: wire button signals -----------------------------------
	_push_menu_btn.pressed.connect(_push_menu)
	_push_gameplay_btn.pressed.connect(_push_gameplay)
	_push_paused_btn.pressed.connect(_push_paused)
	_pop_btn.pressed.connect(_pop_top)
	_clear_btn.pressed.connect(_clear_all)
	_log("[color=green]Demo ready. Press Space/Enter/Esc or use the buttons.[/color]")

	# --- 5. Start in Menu mode (only IMC_Menu on the stack) ---------
	_push_menu()


# ---------------------------------------------------------------------------
# Trigger event handlers — bound to (action, event) triples via
# `ei.bind_action(...)` in _ready.
# ---------------------------------------------------------------------------

func _on_drop(_action, _event, _value) -> void:
	# Rising-edge handler — this is "OnJump" for a real Plinko game.
	_log("[color=lime]→ IA_Drop STARTED (OnJump!)[/color]")
	_score_drop += 1

func _on_drop_repeat(_action, _event, _value) -> void:
	# Key-repeat / multi-press handler. Useful for charging meters or
	# "press harder" effects. In a Plinko game we'd ignore this; the
	# demo logs it for educational value.
	_log("[color=yellow]→ IA_Drop TRIGGERED (key-repeat)[/color]")

func _on_drop_released(_action, _event, _value) -> void:
	_log("[color=yellow]→ IA_Drop COMPLETED (Space released)[/color]")

func _on_pause(_action, _event, _value) -> void:
	_log("[color=orange]→ IA_Pause STARTED (pushing IMC_Paused)[/color]")
	_push_imc(_imc_paused, 20)
	_refresh_status()

func _on_resume(_action, _event, _value) -> void:
	_log("[color=cyan]→ IA_Resume STARTED (popping top IMC if Paused)[/color]")
	# Only pop if the top of the stack is Paused. This guards against
	# triggering Resume from Menu mode (where Resume is the only thing
	# the IMC binds — and there's nothing to pop).
	if _active_imcs.size() > 0 and _active_imcs[-1] == _imc_paused:
		_pop_top()
	else:
		_log("  (top of stack is not Paused; nothing to pop)")


# ---------------------------------------------------------------------------
# Button handlers — manually push/pop the IMC stack
# ---------------------------------------------------------------------------

func _push_menu() -> void:
	# Menu mode: only the Menu IMC, no Gameplay or Paused on top.
	_clear_all()
	_push_imc(_imc_menu, 0)
	_refresh_status()

func _push_gameplay() -> void:
	_clear_all()
	# Gameplay sits above Menu in priority (10 > 0), but to keep the
	# demo deterministic we don't stack them — just Gameplay alone.
	_push_imc(_imc_gameplay, 10)
	_refresh_status()

func _push_paused() -> void:
	# Push Paused on top of whatever's there. Useful for showing the
	# layered (IMC stack) behavior.
	_push_imc(_imc_paused, 20)
	_refresh_status()

func _pop_top() -> void:
	if _active_imcs.is_empty():
		_log("[color=gray](stack empty; nothing to pop)[/color]")
		return
	var top = _active_imcs.pop_back()
	var ei: Object = Engine.get_singleton("EISubsystem")
	if ei:
		ei.remove_mapping_context(top)
	_log("[color=gray]popped %s[/color]" % top.context_name)
	_refresh_status()

func _clear_all() -> void:
	var ei: Object = Engine.get_singleton("EISubsystem")
	if ei:
		ei.clear_all_mapping_contexts()
	_active_imcs.clear()
	_log("[color=gray]cleared all IMCs[/color]")
	_refresh_status()


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

func _push_imc(p_imc: Resource, p_priority: int) -> void:
	var ei: Object = Engine.get_singleton("EISubsystem")
	if ei == null:
		_log("[color=red]EISubsystem missing on _push_imc[/color]")
		return
	ei.add_mapping_context(p_imc, p_priority)
	_active_imcs.append(p_imc)
	_log("[color=green]pushed %s @ priority %d[/color]" % [p_imc.context_name, p_priority])

func _refresh_status() -> void:
	var names: Array = []
	for imc in _active_imcs:
		names.append(imc.context_name)
	_status_label.text = "Active IMCs (bottom→top): %s  |  drops: %d" % [
		" → ".join(names) if names else "(none)",
		_score_drop,
	]

func _log(p_msg: String) -> void:
	# RichTextLabel with bbcode_enabled=true in the .tscn handles the
	# [color=...] tags. Append with a newline.
	if _log_label:
		_log_label.append_text(p_msg + "\n")
	else:
		# Booting headless before UI is up: print to stdout.
		print(p_msg)
