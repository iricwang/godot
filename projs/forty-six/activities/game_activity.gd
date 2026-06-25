extends Activity
## GameActivity — the click-counter game itself.
##
## Demonstrates:
##   * Two-way feel via VM bulk-updates: count/progress/elapsed all batched.
##   * show_toast_with_owner — milestone announcements scoped to this Activity
##     (auto-cancelled if the user backs out mid-run).
##   * show_dialog (pause) wired so the Pause Dialog calls back into us by name.
##   * Intent extras — passing the score forward to GameOverActivity.
##   * FLAG_NO_HISTORY on the GameOver push so Back from GameOver returns to
##     the MainMenu, not this finished GameActivity.
##   * load_resource_async — kicks off a background load just to exercise the
##     ResourceManager polling path (the result is logged).
##   * ui_cancel input → show pause dialog (not finish()) — Android-style.
##   * Lifecycle: pause stops the timer; resume restarts it from where it left off.
##   * enhanced_input: Space → click (also bound under EI so the Click action
##     can be re-mapped in one place); Esc → pause dialog.

const GameVM := preload("res://view_models/game_vm.gd")
const EIHelpers := preload("res://core/ei_helpers.gd")
const MILESTONES := [10, 20, 30, 40]

var vm: GameVM
var _count_label: Label
var _target_label: Label
var _elapsed_label: Label
var _status_label: Label
var _progress: ProgressBar
var _click_btn: Button
var _pause_btn: Button

var _target_clicks: int = 46
var _elapsed: float = 0.0
var _running: bool = false
var _emitted_milestones: Array[int] = []

var _ia_click: Resource
var _ia_pause: Resource
var _imc: Resource
var _ei_component: Node


func _on_create(_saved_state: Dictionary) -> void:
	var tin := Transition.new(); tin.enter_type = Transition.SLIDE_LEFT; tin.duration = 0.25
	var tout := Transition.new(); tout.exit_type = Transition.SLIDE_RIGHT; tout.duration = 0.25
	transition_in = tin
	transition_out = tout

	# Difficulty comes from the GameStateService — different activities can share state.
	var gs = get_service(&"game_state")
	_target_clicks = gs.get_target_clicks() if gs else 46

	vm = GameVM.new()
	vm.set_target(_target_clicks)
	_build_ui()
	_bind()
	_setup_keyboard()

	# Exercise async resource loading — purely demonstrative.
	load_resource_async("res://icon.svg", _on_icon_loaded)


func _on_resume() -> void:
	_running = true
	EIHelpers.add_context(_imc, 10)


func _on_pause() -> void:
	_running = false
	EIHelpers.remove_context(_imc)


func _on_destroy() -> void:
	EIHelpers.remove_context(_imc)
	if vm:
		vm.dispose()


## ESC / Android Back — show pause dialog instead of finishing the run.
## Return true so ActivityManager doesn't pop us.
func _on_back_pressed() -> bool:
	_show_pause()
	return true


func _process(delta: float) -> void:
	if not _running:
		return
	_elapsed += delta
	vm.set_elapsed(_elapsed)


func _build_ui() -> void:
	var root_vb := VBoxContainer.new()
	root_vb.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_vb.add_theme_constant_override("separation", 10)
	add_child(root_vb)

	# Top bar — pause + elapsed.
	var top := HBoxContainer.new()
	top.custom_minimum_size = Vector2(0, 48)
	root_vb.add_child(top)
	_pause_btn = Button.new()
	_pause_btn.text = "⏸  Pause  [Esc]"
	_pause_btn.pressed.connect(_show_pause)
	top.add_child(_pause_btn)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(spacer)
	_elapsed_label = Label.new()
	_elapsed_label.add_theme_font_size_override("font_size", 22)
	top.add_child(_elapsed_label)

	# Big count.
	var center := VBoxContainer.new()
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	center.alignment = BoxContainer.ALIGNMENT_CENTER
	root_vb.add_child(center)

	var hb := HBoxContainer.new()
	hb.alignment = BoxContainer.ALIGNMENT_CENTER
	hb.add_theme_constant_override("separation", 8)
	center.add_child(hb)
	_count_label = Label.new()
	_count_label.add_theme_font_size_override("font_size", 96)
	hb.add_child(_count_label)
	_target_label = Label.new()
	_target_label.add_theme_font_size_override("font_size", 32)
	_target_label.modulate = Color(0.6, 0.6, 0.7)
	hb.add_child(_target_label)

	_status_label = Label.new()
	_status_label.add_theme_font_size_override("font_size", 16)
	_status_label.modulate = Color(0.75, 0.78, 0.85)
	_status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	center.add_child(_status_label)

	# Progress.
	var bar_margin := MarginContainer.new()
	bar_margin.add_theme_constant_override("margin_left", 40)
	bar_margin.add_theme_constant_override("margin_right", 40)
	bar_margin.add_theme_constant_override("margin_top", 8)
	bar_margin.add_theme_constant_override("margin_bottom", 8)
	root_vb.add_child(bar_margin)
	_progress = ProgressBar.new()
	_progress.min_value = 0.0
	_progress.max_value = 1.0
	_progress.step = 0.001
	_progress.show_percentage = false
	_progress.custom_minimum_size = Vector2(0, 18)
	bar_margin.add_child(_progress)

	# Click button.
	var bottom_margin := MarginContainer.new()
	bottom_margin.add_theme_constant_override("margin_left", 40)
	bottom_margin.add_theme_constant_override("margin_right", 40)
	bottom_margin.add_theme_constant_override("margin_bottom", 32)
	root_vb.add_child(bottom_margin)
	_click_btn = Button.new()
	_click_btn.text = "Click!  [Space]"
	_click_btn.custom_minimum_size = Vector2(0, 80)
	_click_btn.add_theme_font_size_override("font_size", 28)
	_click_btn.pressed.connect(_on_click)
	bottom_margin.add_child(_click_btn)


func _bind() -> void:
	Context.bind_property(_count_label, "text", vm, "count_label")
	Context.bind_property(_target_label, "text", vm, "target_label")
	Context.bind_property(_elapsed_label, "text", vm, "elapsed_label")
	Context.bind_property(_status_label, "text", vm, "status")
	Context.bind_property(_progress, "value", vm, "progress")


func _on_click() -> void:
	if not _running:
		_running = true
		_elapsed = 0.0
		vm.status = "Go!"
	if has_service(&"audio"):
		get_service(&"audio").play_sfx(&"click")
	vm.bump()
	var c := int(vm.count)
	# Milestone toast — owner = this Activity → auto-cancelled if user backs out.
	if c in MILESTONES and not _emitted_milestones.has(c):
		_emitted_milestones.append(c)
		show_toast(Context.make_toast("%d / %d  —  keep going!" % [c, _target_clicks], 1.2))
	if c >= _target_clicks:
		_finish_run()


func _finish_run() -> void:
	_running = false
	var gs = get_service(&"game_state")
	var is_new := false
	if gs:
		is_new = gs.record_run(_elapsed)
	show_toast(Context.make_toast("✓  Run complete", 1.5))
	# FLAG_NO_HISTORY so back-from-GameOver returns to MainMenu (not this finished game).
	var ix := Intent.create("game_over", Intent.FLAG_NO_HISTORY, {
		"score": _elapsed,
		"target": _target_clicks,
		"new_record": is_new,
	})
	start_activity(ix)


# ---- Pause dialog plumbing ----
# The PauseDialog has no return value — it calls these methods on us (its
# lifecycle owner) via Object.call(). This pattern keeps the dialog dumb and
# avoids passing closures through Intent.extras.

func _show_pause() -> void:
	_running = false
	vm.status = "Paused"
	show_dialog(Intent.create("pause_dialog"))


func on_pause_resume() -> void:
	# Called by PauseDialog after dismiss.
	vm.status = "Go!"
	_running = true


func on_pause_restart() -> void:
	# Restart with the same Activity instance via FLAG_SINGLE_TOP — dispatches _on_new_intent.
	_elapsed = 0.0
	_emitted_milestones.clear()
	vm.begin_bulk_update()
	vm.count = 0
	vm.count_label = "0"
	vm.progress = 0.0
	vm.elapsed_label = "0.00s"
	vm.status = "Restarted — click to start the clock"
	vm.end_bulk_update()
	_running = false


func on_pause_quit_to_menu() -> void:
	start_activity_with("main_menu", Intent.FLAG_NEW_CLEAR)


func _on_icon_loaded(res: Resource) -> void:
	# Just prove the async path completed — we don't actually use the texture.
	if res:
		print("[GameActivity] async icon loaded: %s" % res)
	else:
		print("[GameActivity] async icon load failed (expected if icon.svg missing)")


# ---- Keyboard (via enhanced_input) ----
#
# IA_Click — fires on Space. We route through EI rather than connect
# Input.is_action_just_pressed directly so the binding can later be re-mapped
# without touching this script. IA_Pause shadows the framework's
# `_on_back_pressed` behaviour — Esc opens the pause dialog instead of finishing
# the run. Both keys are claimed with consumes=true so they don't leak through.

func _setup_keyboard() -> void:
	if not EIHelpers.available():
		return
	_ia_click = EIHelpers.make_bool_action("Game.Click")
	_ia_pause = EIHelpers.make_bool_action("Game.Pause")
	_imc = EIHelpers.make_context("IMC_Game")
	_imc.add_mapping(EIHelpers.key_event(KEY_SPACE), _ia_click, [], [], true)
	# Allow Enter as an alias for the click — fits the click-spamming theme.
	_imc.add_mapping(EIHelpers.key_event(KEY_ENTER), _ia_click, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_ESCAPE), _ia_pause, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_P), _ia_pause, [], [], true)

	_ei_component = EIHelpers.attach_component(self)
	_ei_component.bind(_ia_click, EIHelpers.TRIG_STARTED, _on_click)
	_ei_component.bind(_ia_pause, EIHelpers.TRIG_STARTED, _show_pause)
