extends Activity
## SettingsActivity — difficulty + volume.
##
## Demonstrates:
##   * Read/write of a shared service (GameStateService) across Activities.
##   * View → VM via explicit widget signal handlers (the "back-channel" since
##     BindingEngine is VM → View only).
##   * AudioService side-effect on volume change — services can collaborate.
##   * back() / ui_cancel returns to MainMenu via FLAG_SINGLE_TOP.
##   * Transition: FADE both ways for a low-key feel.
##   * enhanced_input: Left/Right cycle difficulty, Up/Down (+/-) adjust volume
##     by 10, R resets the best time, Esc/Backspace → Main Menu.

const SettingsVM := preload("res://view_models/settings_vm.gd")
const EIHelpers := preload("res://core/ei_helpers.gd")
const Responsive := preload("res://core/responsive.gd")
const DIFFICULTIES := ["Easy (23)", "Normal (46)", "Hard (92)"]

var vm: SettingsVM
var _diff_label: Label
var _diff_opt: OptionButton
var _vol_label: Label
var _vol_slider: HSlider
var _back_btn: Button
var _reset_btn: Button
var _hint: Label

var _ia_prev_diff: Resource
var _ia_next_diff: Resource
var _ia_vol_up: Resource
var _ia_vol_down: Resource
var _ia_reset: Resource
var _ia_back: Resource
var _imc: Resource
var _ei_component: Node


func _on_create(_saved_state: Dictionary) -> void:
	var tin := Transition.new(); tin.enter_type = Transition.FADE; tin.duration = 0.2
	var tout := Transition.new(); tout.exit_type = Transition.FADE; tout.duration = 0.2
	transition_in = tin
	transition_out = tout

	vm = SettingsVM.new()
	_build_ui()
	_bind()
	_setup_keyboard()

	var gs = get_service(&"game_state")
	if gs:
		vm.sync_from(gs)
		# Reflect the current selection in the widgets — bind_property updates labels
		# automatically, but OptionButton.selected / Range.value need a one-time sync.
		_diff_opt.selected = int(vm.difficulty)
		_vol_slider.value = float(vm.volume)


func _on_resume() -> void:
	EIHelpers.add_context(_imc, 10)


func _on_pause() -> void:
	EIHelpers.remove_context(_imc)


func _on_destroy() -> void:
	EIHelpers.remove_context(_imc)
	if vm:
		vm.dispose()


func _on_back_pressed() -> bool:
	# Return to MainMenu with SINGLE_TOP so we don't pile up menus.
	start_activity_with("main_menu", Intent.FLAG_SINGLE_TOP)
	return true


func _build_ui() -> void:
	var vb := VBoxContainer.new()
	vb.set_anchors_preset(Control.PRESET_CENTER)
	vb.add_theme_constant_override("separation", Responsive.gap(16, self))
	# Capped at 420 (the desktop design), but never exceed available width
	# minus gutters -- so a 393-wide phone gets ~369 instead of overflowing.
	vb.custom_minimum_size = Vector2(Responsive.panel_width(420, self), 0)
	add_child(vb)

	var title := Label.new()
	title.text = "⚙  Settings"
	title.add_theme_font_size_override("font_size", Responsive.font(36, self))
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(title)

	var sep := Control.new()
	sep.custom_minimum_size = Vector2(0, Responsive.gap(16, self))
	vb.add_child(sep)

	# Difficulty.
	_diff_label = Label.new()
	_diff_label.add_theme_font_size_override("font_size", Responsive.font(16, self))
	vb.add_child(_diff_label)
	_diff_opt = OptionButton.new()
	_diff_opt.custom_minimum_size = Responsive.button_min(Vector2(0, 40), self)
	for n in DIFFICULTIES:
		_diff_opt.add_item(n)
	_diff_opt.item_selected.connect(_on_difficulty_changed)
	vb.add_child(_diff_opt)

	var sep2 := Control.new()
	sep2.custom_minimum_size = Vector2(0, 8)
	vb.add_child(sep2)

	# Volume.
	_vol_label = Label.new()
	_vol_label.add_theme_font_size_override("font_size", Responsive.font(16, self))
	vb.add_child(_vol_label)
	_vol_slider = HSlider.new()
	_vol_slider.min_value = 0
	_vol_slider.max_value = 100
	_vol_slider.step = 1
	# A taller slider track on mobile makes it actually grabbable with a thumb.
	_vol_slider.custom_minimum_size = Vector2(0, 32 if (Responsive.is_compact(self) or Responsive.is_mobile()) else 20)
	_vol_slider.value_changed.connect(_on_volume_changed)
	vb.add_child(_vol_slider)

	var sep3 := Control.new()
	sep3.custom_minimum_size = Vector2(0, Responsive.gap(24, self))
	vb.add_child(sep3)

	# Buttons row.
	var hb := HBoxContainer.new()
	hb.alignment = BoxContainer.ALIGNMENT_CENTER
	hb.add_theme_constant_override("separation", 12)
	vb.add_child(hb)
	_reset_btn = Button.new()
	_reset_btn.text = "Reset Best  [R]"
	_reset_btn.custom_minimum_size = Responsive.button_min(Vector2(0, 36), self)
	_reset_btn.pressed.connect(_on_reset_best)
	hb.add_child(_reset_btn)
	_back_btn = Button.new()
	_back_btn.text = "← Back  [Esc]"
	_back_btn.custom_minimum_size = Responsive.button_min(Vector2(0, 36), self)
	_back_btn.pressed.connect(_on_back)
	hb.add_child(_back_btn)

	var sep4 := Control.new()
	sep4.custom_minimum_size = Vector2(0, 8)
	vb.add_child(sep4)

	_hint = Label.new()
	# On compact viewports the long hint wraps mid-row and looks noisy; hide
	# it (the keys are also surfaced on each control's label).
	if Responsive.is_compact(self):
		_hint.text = ""
		_hint.visible = false
	else:
		_hint.text = "Keys: ←/→ difficulty   ↑↓ or +/- volume   R reset   Esc back"
	_hint.add_theme_font_size_override("font_size", Responsive.font(11, self))
	_hint.modulate = Color(0.6, 0.62, 0.7)
	_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_hint)


func _bind() -> void:
	Context.bind_property(_diff_label, "text", vm, "difficulty_label")
	Context.bind_property(_vol_label, "text", vm, "volume_label")


func _on_difficulty_changed(idx: int) -> void:
	var name: String = DIFFICULTIES[idx] if idx >= 0 and idx < DIFFICULTIES.size() else "?"
	vm.set_difficulty_local(idx, name)
	var gs = get_service(&"game_state")
	if gs:
		gs.difficulty = idx
	show_toast(Context.make_toast("Difficulty → %s" % name, 1.2))


func _on_volume_changed(value: float) -> void:
	var v := int(value)
	vm.set_volume_local(v)
	var gs = get_service(&"game_state")
	if gs:
		gs.volume = v
	if has_service(&"audio"):
		get_service(&"audio").set_volume(v)


func _on_reset_best() -> void:
	var gs = get_service(&"game_state")
	if gs:
		gs.reset_best()
		show_toast(Context.make_toast("Best time cleared", 1.2))


func _on_back() -> void:
	start_activity_with("main_menu", Intent.FLAG_SINGLE_TOP)


# ---- Keyboard ----

func _setup_keyboard() -> void:
	if not EIHelpers.available():
		return
	_ia_prev_diff = EIHelpers.make_bool_action("Settings.PrevDifficulty")
	_ia_next_diff = EIHelpers.make_bool_action("Settings.NextDifficulty")
	_ia_vol_up = EIHelpers.make_bool_action("Settings.VolUp")
	_ia_vol_down = EIHelpers.make_bool_action("Settings.VolDown")
	_ia_reset = EIHelpers.make_bool_action("Settings.ResetBest")
	_ia_back = EIHelpers.make_bool_action("Settings.Back")
	_imc = EIHelpers.make_context("IMC_Settings")
	_imc.add_mapping(EIHelpers.key_event(KEY_LEFT), _ia_prev_diff, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_RIGHT), _ia_next_diff, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_UP), _ia_vol_up, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_DOWN), _ia_vol_down, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_EQUAL), _ia_vol_up, [], [], true)   # + (also =)
	_imc.add_mapping(EIHelpers.key_event(KEY_MINUS), _ia_vol_down, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_R), _ia_reset, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_ESCAPE), _ia_back, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_BACKSPACE), _ia_back, [], [], true)

	_ei_component = EIHelpers.attach_component(self)
	_ei_component.bind(_ia_prev_diff, EIHelpers.TRIG_STARTED, _on_prev_difficulty_key)
	_ei_component.bind(_ia_next_diff, EIHelpers.TRIG_STARTED, _on_next_difficulty_key)
	_ei_component.bind(_ia_vol_up, EIHelpers.TRIG_STARTED, _on_vol_up_key)
	_ei_component.bind(_ia_vol_down, EIHelpers.TRIG_STARTED, _on_vol_down_key)
	_ei_component.bind(_ia_reset, EIHelpers.TRIG_STARTED, _on_reset_best)
	_ei_component.bind(_ia_back, EIHelpers.TRIG_STARTED, _on_back)


func _on_prev_difficulty_key() -> void:
	var idx: int = (int(vm.difficulty) - 1 + DIFFICULTIES.size()) % DIFFICULTIES.size()
	_diff_opt.selected = idx
	_on_difficulty_changed(idx)


func _on_next_difficulty_key() -> void:
	var idx: int = (int(vm.difficulty) + 1) % DIFFICULTIES.size()
	_diff_opt.selected = idx
	_on_difficulty_changed(idx)


func _on_vol_up_key() -> void:
	var v: int = clamp(int(vm.volume) + 10, 0, 100)
	_vol_slider.value = v
	_on_volume_changed(v)


func _on_vol_down_key() -> void:
	var v: int = clamp(int(vm.volume) - 10, 0, 100)
	_vol_slider.value = v
	_on_volume_changed(v)
