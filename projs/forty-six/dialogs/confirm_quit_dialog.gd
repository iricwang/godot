extends Dialog
## ConfirmQuitDialog — Yes/No on the way out.
##
## On Yes → quits the SceneTree (Application.shutdown auto-fires from
## NOTIFICATION_EXIT_TREE on the Application node, draining the activity stack).
##
## enhanced_input: Enter/Y → Yes, Esc/N → No (priority 100 to overshadow MainMenu).

const EIHelpers := preload("res://core/ei_helpers.gd")

var _ia_yes: Resource
var _ia_no: Resource
var _imc: Resource
var _ei_component: Node


func _on_create(_saved_state: Dictionary) -> void:
	var tin := Transition.new(); tin.enter_type = Transition.SCALE; tin.duration = 0.18
	var tout := Transition.new(); tout.exit_type = Transition.FADE; tout.duration = 0.15
	transition_in = tin
	transition_out = tout

	var backdrop := ColorRect.new()
	backdrop.color = Color(0, 0, 0, 0.55)
	backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(backdrop)

	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.custom_minimum_size = Vector2(320, 0)
	add_child(panel)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 22)
	margin.add_theme_constant_override("margin_right", 22)
	margin.add_theme_constant_override("margin_top", 22)
	margin.add_theme_constant_override("margin_bottom", 22)
	panel.add_child(margin)

	var vb := VBoxContainer.new()
	vb.add_theme_constant_override("separation", 14)
	margin.add_child(vb)

	var title := Label.new()
	title.text = "Quit FortySix?"
	title.add_theme_font_size_override("font_size", 22)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(title)

	var sub := Label.new()
	sub.text = "Your best time is kept for the session."
	sub.modulate = Color(0.7, 0.72, 0.78)
	sub.add_theme_font_size_override("font_size", 13)
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	vb.add_child(sub)

	var hb := HBoxContainer.new()
	hb.alignment = BoxContainer.ALIGNMENT_CENTER
	hb.add_theme_constant_override("separation", 16)
	vb.add_child(hb)

	var no := Button.new()
	no.text = "Cancel  [Esc/N]"
	no.custom_minimum_size = Vector2(140, 36)
	no.pressed.connect(_on_no)
	hb.add_child(no)

	var yes := Button.new()
	yes.text = "Quit  [Enter/Y]"
	yes.custom_minimum_size = Vector2(140, 36)
	yes.pressed.connect(_on_yes)
	hb.add_child(yes)

	_setup_keyboard()


func _on_dismiss() -> void:
	EIHelpers.remove_context(_imc)


func _setup_keyboard() -> void:
	if not EIHelpers.available():
		return
	_ia_yes = EIHelpers.make_bool_action("ConfirmQuit.Yes")
	_ia_no = EIHelpers.make_bool_action("ConfirmQuit.No")
	_imc = EIHelpers.make_context("IMC_ConfirmQuit")
	_imc.add_mapping(EIHelpers.key_event(KEY_ENTER), _ia_yes, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_Y), _ia_yes, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_ESCAPE), _ia_no, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_N), _ia_no, [], [], true)

	_ei_component = EIHelpers.attach_component(self)
	_ei_component.bind(_ia_yes, EIHelpers.TRIG_STARTED, _on_yes)
	_ei_component.bind(_ia_no, EIHelpers.TRIG_STARTED, _on_no)
	EIHelpers.add_context(_imc, 100)


func _on_no() -> void:
	dismiss()


func _on_yes() -> void:
	dismiss()
	# Defer the quit so the dismiss transition has a frame to settle.
	get_tree().call_deferred("quit")
