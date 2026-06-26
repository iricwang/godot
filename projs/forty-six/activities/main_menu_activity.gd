extends Activity
## MainMenuActivity — title screen.
##
## Demonstrates:
##   * MVVM: title / subtitle / best_label / runs_label bound via [BindingEngine].
##   * ServiceRegistry: pulls GameStateService to read best_time / total_runs.
##   * Navigation: start_activity_with("game", FLAG_NEW_CLEAR) on Start button;
##                 start_activity_with("settings", FLAG_SINGLE_TOP) on Settings.
##                 Note: button callbacks are named `_on_<name>_pressed` to avoid
##                 colliding with Activity's lifecycle virtuals (e.g. `_on_start`).
##   * Lifecycle: on_resume refreshes the VM so a returning user sees the
##     updated best-time without rebuilding the UI.
##   * Toast: "Welcome back" emitted on resume from another Activity.
##   * Transition: SLIDE_RIGHT enter / SLIDE_LEFT exit (paired with Game's reverse).
##   * enhanced_input: Enter/Space → Start, S → Settings, Q/Esc → Quit dialog.

const MainMenuVM := preload("res://view_models/main_menu_vm.gd")
const EIHelpers := preload("res://core/ei_helpers.gd")
const Responsive := preload("res://core/responsive.gd")

var vm: MainMenuVM
var _title: Label
var _subtitle: Label
var _best: Label
var _runs: Label
var _start_btn: Button
var _settings_btn: Button
var _quit_btn: Button

# enhanced_input resources — built once in _on_create, registered in
# _on_resume and deregistered in _on_pause so only the top Activity owns
# the keyboard.
var _ia_start: Resource
var _ia_settings: Resource
var _ia_quit: Resource
var _imc: Resource
var _ei_component: Node


func _on_create(_saved_state: Dictionary) -> void:
	# Slide-from-right to enter, slide-to-left to exit so going DEEPER feels rightward.
	var tin := Transition.new()
	tin.enter_type = Transition.SLIDE_RIGHT
	tin.duration = 0.25
	transition_in = tin
	var tout := Transition.new()
	tout.exit_type = Transition.SLIDE_LEFT
	tout.duration = 0.25
	transition_out = tout

	vm = MainMenuVM.new()
	_build_ui()
	_bind()
	_setup_keyboard()
	_refresh_from_state()


func _on_resume() -> void:
	# Coming back from Game or Settings — refresh stats + reclaim the keyboard.
	_refresh_from_state()
	EIHelpers.add_context(_imc, 10)
	if has_service(&"audio"):
		get_service(&"audio").play_sfx(&"menu_resume")


func _on_pause() -> void:
	# Another activity took the foreground — release our shortcuts so its keys
	# don't double-fire ours.
	EIHelpers.remove_context(_imc)


func _on_destroy() -> void:
	EIHelpers.remove_context(_imc)
	if vm:
		vm.dispose()


func _refresh_from_state() -> void:
	if not has_service(&"game_state"):
		return
	var gs = get_service(&"game_state")
	vm.refresh_from_state(gs)


func _build_ui() -> void:
	var vb := VBoxContainer.new()
	vb.set_anchors_preset(Control.PRESET_CENTER)
	vb.add_theme_constant_override("separation", Responsive.gap(14, self))
	add_child(vb)

	_title = Label.new()
	_title.add_theme_font_size_override("font_size", Responsive.font(56, self))
	_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_title)

	_subtitle = Label.new()
	_subtitle.add_theme_font_size_override("font_size", Responsive.font(18, self))
	_subtitle.modulate = Color(0.75, 0.78, 0.85)
	_subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_subtitle)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, Responsive.gap(24, self))
	vb.add_child(spacer)

	_start_btn = _make_menu_button("▶  Start  [Enter/Space]", _on_start_pressed)
	vb.add_child(_start_btn)
	_settings_btn = _make_menu_button("⚙  Settings  [S]", _on_settings_pressed)
	vb.add_child(_settings_btn)
	_quit_btn = _make_menu_button("⏻  Quit  [Q/Esc]", _on_quit_pressed)
	vb.add_child(_quit_btn)

	var spacer2 := Control.new()
	spacer2.custom_minimum_size = Vector2(0, Responsive.gap(24, self))
	vb.add_child(spacer2)

	_best = Label.new()
	_best.add_theme_font_size_override("font_size", Responsive.font(14, self))
	_best.modulate = Color(1.0, 0.9, 0.5)
	_best.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_best)

	_runs = Label.new()
	_runs.add_theme_font_size_override("font_size", Responsive.font(12, self))
	_runs.modulate = Color(0.65, 0.65, 0.72)
	_runs.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_runs)


func _make_menu_button(text: String, cb: Callable) -> Button:
	var b := Button.new()
	b.text = text
	# Touch-friendly height on phones; preserve the 240px design width as the
	# min so the menu reads consistently across breakpoints.
	b.custom_minimum_size = Responsive.button_min(Vector2(240, 44), self)
	b.add_theme_font_size_override("font_size", Responsive.font(18, self))
	b.pressed.connect(cb)
	return b


func _bind() -> void:
	# MVVM: one-way VM → View binding via the C++ BindingEngine.
	Context.bind_property(_title, "text", vm, "title")
	Context.bind_property(_subtitle, "text", vm, "subtitle")
	Context.bind_property(_best, "text", vm, "best_label")
	Context.bind_property(_runs, "text", vm, "runs_label")


func _on_start_pressed() -> void:
	# FLAG_NEW_CLEAR — drop the back stack so Back from GameOver lands on a fresh MainMenu.
	# (The intent passes nothing; GameActivity reads its difficulty from the service.)
	start_activity_with("game", Intent.FLAG_NEW_CLEAR)


func _on_settings_pressed() -> void:
	# FLAG_SINGLE_TOP — if user mashes Settings, reuse the existing instance.
	start_activity_with("settings", Intent.FLAG_SINGLE_TOP)


func _on_quit_pressed() -> void:
	# Dialog with `this` as owner — auto-dismissed if MainMenu is destroyed.
	show_dialog(Intent.create("confirm_quit_dialog"))


func _setup_keyboard() -> void:
	# Build the IMC once; we add/remove it on resume/pause so only the visible
	# Activity owns the keys. EIComponent is attached as a child Node so its
	# spec §4.7 _exit_tree teardown auto-unbinds our callbacks if we leak it.
	if not EIHelpers.available():
		return
	_ia_start = EIHelpers.make_bool_action("MainMenu.Start")
	_ia_settings = EIHelpers.make_bool_action("MainMenu.Settings")
	_ia_quit = EIHelpers.make_bool_action("MainMenu.Quit")
	_imc = EIHelpers.make_context("IMC_MainMenu")
	# consumes=true on each — keep the events from leaking out to any other IMC.
	_imc.add_mapping(EIHelpers.key_event(KEY_ENTER), _ia_start, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_SPACE), _ia_start, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_S), _ia_settings, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_Q), _ia_quit, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_ESCAPE), _ia_quit, [], [], true)

	_ei_component = EIHelpers.attach_component(self)
	_ei_component.bind(_ia_start, EIHelpers.TRIG_STARTED, _on_start_pressed)
	_ei_component.bind(_ia_settings, EIHelpers.TRIG_STARTED, _on_settings_pressed)
	_ei_component.bind(_ia_quit, EIHelpers.TRIG_STARTED, _on_quit_pressed)
