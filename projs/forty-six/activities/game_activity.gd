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
const Responsive := preload("res://core/responsive.gd")
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
# Snapshot of `_running` at the moment _on_pause was dispatched. Used to
# restore the timer state when _on_resume fires (e.g. after a SceneActivity
# curtain lifts) -- otherwise we'd resume the timer even if the user had
# already paused via the dialog before the scene took over.
var _was_running_before_pause: bool = false
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
	# Restore the exact timer state the user had before we got paused, NOT
	# an unconditional `true`. This matters when a SceneActivity curtain
	# lifts while the PauseDialog is still visible on top of us -- the
	# user expected to be paused under the dialog, so we must not start
	# ticking again just because the activity got resumed.
	_running = _was_running_before_pause
	EIHelpers.add_context(_imc, 10)


func _on_pause() -> void:
	# Save the pre-pause state so _on_resume can restore it precisely.
	_was_running_before_pause = _running
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
	# Detected once per Activity instance. If the user toggles
	# Resolution/Orientation in the editor mid-play, the OS window resizes
	# but content_scale_size stays at the value baked when this Activity
	# was created -- so these decisions don't go stale within a run.
	var compact: bool = Responsive.is_compact(self)
	var portrait_stack: bool = Responsive.stack_vertically(self)
	var gutter_px: int = Responsive.gutter(self)

	var root_vb := VBoxContainer.new()
	root_vb.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_vb.add_theme_constant_override("separation", Responsive.gap(10, self))
	add_child(root_vb)

	# Top bar — pause + elapsed.
	var top := HBoxContainer.new()
	top.custom_minimum_size = Vector2(0, 52 if compact else 48)
	root_vb.add_child(top)
	_pause_btn = Button.new()
	_pause_btn.text = "⏸  Pause" if compact else "⏸  Pause  [Esc]"
	_pause_btn.custom_minimum_size = Responsive.button_min(Vector2(0, 40), self)
	_pause_btn.pressed.connect(_show_pause)
	top.add_child(_pause_btn)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(spacer)
	_elapsed_label = Label.new()
	_elapsed_label.add_theme_font_size_override("font_size", Responsive.font(22, self))
	top.add_child(_elapsed_label)

	# Big count.
	var center := VBoxContainer.new()
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	center.alignment = BoxContainer.ALIGNMENT_CENTER
	root_vb.add_child(center)

	# Count vs target: side-by-side on landscape/desktop, stacked on a
	# portrait phone where horizontal width is the scarce axis.
	var cluster: BoxContainer
	if portrait_stack:
		cluster = VBoxContainer.new()
	else:
		cluster = HBoxContainer.new()
	cluster.alignment = BoxContainer.ALIGNMENT_CENTER
	cluster.add_theme_constant_override("separation", 8)
	center.add_child(cluster)
	_count_label = Label.new()
	_count_label.add_theme_font_size_override("font_size", Responsive.font(96, self))
	_count_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cluster.add_child(_count_label)
	_target_label = Label.new()
	_target_label.add_theme_font_size_override("font_size", Responsive.font(32, self))
	_target_label.modulate = Color(0.6, 0.6, 0.7)
	_target_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cluster.add_child(_target_label)

	_status_label = Label.new()
	_status_label.add_theme_font_size_override("font_size", Responsive.font(16, self))
	_status_label.modulate = Color(0.75, 0.78, 0.85)
	_status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	center.add_child(_status_label)

	# Progress.
	var bar_margin := MarginContainer.new()
	bar_margin.add_theme_constant_override("margin_left", gutter_px)
	bar_margin.add_theme_constant_override("margin_right", gutter_px)
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
	bottom_margin.add_theme_constant_override("margin_left", gutter_px)
	bottom_margin.add_theme_constant_override("margin_right", gutter_px)
	bottom_margin.add_theme_constant_override("margin_bottom", 24 if compact else 32)
	root_vb.add_child(bottom_margin)
	_click_btn = Button.new()
	_click_btn.text = "Click!" if compact else "Click!  [Space]"
	# 80px is desktop-comfortable; phones need at least 64 to feel tappable
	# in the bottom-of-screen thumb zone.
	_click_btn.custom_minimum_size = Vector2(0, 64 if compact else 80)
	_click_btn.add_theme_font_size_override("font_size", Responsive.font(28, self))
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
