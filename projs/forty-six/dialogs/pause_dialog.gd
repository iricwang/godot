extends Dialog
## PauseDialog — modal pause menu for GameActivity.
##
## Communicates back to the calling Activity via [method get_lifecycle_owner]:
## the Dialog doesn't know which Activity opened it, only that the owner has
## the three [code]on_pause_*[/code] methods. This keeps the dialog reusable.
##
## Transitions: SCALE in / FADE out, matching the "popup" feel from Dialog docs.
##
## enhanced_input: Enter → Resume, R → Restart, M/Esc → Quit to Menu. Registered
## at priority 100 so the Game's priority-10 IMC stops seeing those keys while
## the dialog is open.

const EIHelpers := preload("res://core/ei_helpers.gd")
const Responsive := preload("res://core/responsive.gd")

var _ia_resume: Resource
var _ia_restart: Resource
var _ia_quit: Resource
var _imc: Resource
var _ei_component: Node


func _on_create(_saved_state: Dictionary) -> void:
	var tin := Transition.new(); tin.enter_type = Transition.SCALE; tin.duration = 0.18
	var tout := Transition.new(); tout.exit_type = Transition.FADE; tout.duration = 0.15
	transition_in = tin
	transition_out = tout

	# Semi-transparent backdrop.
	var backdrop := ColorRect.new()
	backdrop.color = Color(0, 0, 0, 0.55)
	backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(backdrop)

	# Centered panel.
	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_CENTER)
	# Was a flat 340; on phones that overflows the gutters, so clamp it.
	panel.custom_minimum_size = Vector2(Responsive.panel_width(340, self), 0)
	add_child(panel)

	var margin := MarginContainer.new()
	var m: int = 18 if Responsive.is_compact(self) else 24
	margin.add_theme_constant_override("margin_left", m)
	margin.add_theme_constant_override("margin_right", m)
	margin.add_theme_constant_override("margin_top", m)
	margin.add_theme_constant_override("margin_bottom", m)
	panel.add_child(margin)

	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", Responsive.gap(12, self))
	margin.add_child(vb)

	var title := Label.new()
	title.text = "⏸  Paused"
	title.add_theme_font_size_override("font_size", Responsive.font(24, self))
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(title)

	var hint := Label.new()
	hint.text = "Take a breath."
	hint.modulate = Color(0.7, 0.72, 0.78)
	hint.add_theme_font_size_override("font_size", Responsive.font(13, self))
	hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(hint)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, Responsive.gap(12, self))
	vb.add_child(spacer)

	_add_btn(vb, "▶  Resume  [Enter]", _on_resume_pressed)
	_add_btn(vb, "↻  Restart  [R]", _on_restart_pressed)
	_add_btn(vb, "☰  Quit to Menu  [M/Esc]", _on_quit_pressed)

	_setup_keyboard()


func _on_dismiss() -> void:
	# Drop the priority-100 IMC so the underlying Activity's keys wake up.
	EIHelpers.remove_context(_imc)


func _on_pause() -> void:
	# A FLAG_SCENE Activity has been pushed on top of us. Surrender the
	# priority-100 IMC so the scene activity sees a clean keyboard; we'll
	# re-add it in _on_resume when the scene curtain lifts.
	EIHelpers.remove_context(_imc)


func _on_resume() -> void:
	# Symmetric to _on_pause. Restore the IMC at the same priority we
	# originally registered with so the dialog's keys win over the
	# underlying Activity again.
	EIHelpers.add_context(_imc, 100)


func _setup_keyboard() -> void:
	if not EIHelpers.available():
		return
	_ia_resume = EIHelpers.make_bool_action("PauseDialog.Resume")
	_ia_restart = EIHelpers.make_bool_action("PauseDialog.Restart")
	_ia_quit = EIHelpers.make_bool_action("PauseDialog.QuitToMenu")
	_imc = EIHelpers.make_context("IMC_PauseDialog")
	_imc.add_mapping(EIHelpers.key_event(KEY_ENTER), _ia_resume, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_SPACE), _ia_resume, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_R), _ia_restart, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_M), _ia_quit, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_ESCAPE), _ia_quit, [], [], true)

	_ei_component = EIHelpers.attach_component(self)
	_ei_component.bind(_ia_resume, EIHelpers.TRIG_STARTED, _on_resume_pressed)
	_ei_component.bind(_ia_restart, EIHelpers.TRIG_STARTED, _on_restart_pressed)
	_ei_component.bind(_ia_quit, EIHelpers.TRIG_STARTED, _on_quit_pressed)
	# Priority 100 > Activity priority 10 so we eat the keys while modal.
	EIHelpers.add_context(_imc, 100)


func _on_setup_standalone(_app: Application) -> void:
	# Allow F6-preview without crashing because the dialog has no owner.
	print("[PauseDialog] standalone preview mode")


func _add_btn(parent: Node, text: String, cb: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Responsive.button_min(Vector2(280, 40), self)
	b.pressed.connect(cb)
	parent.add_child(b)
	return b


# ---- Owner-method dispatch ----
# Looks up the owning Activity (which IS-A Object), checks for the method, calls it.
# Idiomatic Godot — no signal subscriptions needed.

func _dispatch(method: StringName) -> void:
	var owner = get_lifecycle_owner()
	if owner and owner.has_method(method):
		owner.call(method)


func _on_resume_pressed() -> void:
	_dispatch(&"on_pause_resume")
	dismiss()


func _on_restart_pressed() -> void:
	_dispatch(&"on_pause_restart")
	dismiss()


func _on_quit_pressed() -> void:
	_dispatch(&"on_pause_quit_to_menu")
	dismiss()
