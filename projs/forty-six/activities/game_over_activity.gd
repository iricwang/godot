extends Activity
## GameOverActivity — score summary screen.
##
## Demonstrates:
##   * Intent.extras as a result channel — GameActivity passes
##     {score, target, new_record} which we read in _on_create.
##   * Conditional VM: new_record_label only shows when the bool is true.
##   * Navigation: Play Again → start_activity("game", FLAG_NEW_CLEAR);
##                Main Menu → start_activity("main_menu", FLAG_NEW_CLEAR).
##   * Transition: SCALE for arrival, FADE for departure.
##   * enhanced_input: Enter/R → Play Again, Esc/M → Main Menu.

const GameOverVM := preload("res://view_models/game_over_vm.gd")
const EIHelpers := preload("res://core/ei_helpers.gd")
const Responsive := preload("res://core/responsive.gd")

var vm: GameOverVM
var _headline: Label
var _score: Label
var _best: Label
var _new_record: Label
var _again_btn: Button
var _menu_btn: Button

var _ia_again: Resource
var _ia_menu: Resource
var _imc: Resource
var _ei_component: Node


func _on_create(_saved_state: Dictionary) -> void:
	var tin := Transition.new(); tin.enter_type = Transition.SCALE; tin.duration = 0.25
	var tout := Transition.new(); tout.exit_type = Transition.FADE; tout.duration = 0.2
	transition_in = tin
	transition_out = tout

	vm = GameOverVM.new()
	_build_ui()
	_bind()
	_apply_intent()
	_setup_keyboard()


func _on_resume() -> void:
	EIHelpers.add_context(_imc, 10)


func _on_pause() -> void:
	EIHelpers.remove_context(_imc)


func _on_destroy() -> void:
	EIHelpers.remove_context(_imc)
	if vm:
		vm.dispose()


func _apply_intent() -> void:
	var it := get_intent()
	if it == null:
		return
	var extras := it.get_extras()
	var score := float(extras.get("score", 0.0))
	var is_new := bool(extras.get("new_record", false))
	var gs = get_service(&"game_state")
	var best: float = gs.best_time if gs else INF
	vm.apply(score, best, is_new)
	# Visibility: only show the new-record banner when relevant.
	_new_record.visible = is_new


func _build_ui() -> void:
	var vb := VBoxContainer.new()
	vb.set_anchors_preset(Control.PRESET_CENTER)
	vb.add_theme_constant_override("separation", Responsive.gap(14, self))
	add_child(vb)

	_headline = Label.new()
	_headline.add_theme_font_size_override("font_size", Responsive.font(40, self))
	_headline.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_headline)

	_score = Label.new()
	_score.add_theme_font_size_override("font_size", Responsive.font(24, self))
	_score.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_score)

	_best = Label.new()
	_best.add_theme_font_size_override("font_size", Responsive.font(16, self))
	_best.modulate = Color(1.0, 0.9, 0.5)
	_best.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_best)

	_new_record = Label.new()
	_new_record.add_theme_font_size_override("font_size", Responsive.font(20, self))
	_new_record.modulate = Color(0.4, 1.0, 0.6)
	_new_record.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vb.add_child(_new_record)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, Responsive.gap(24, self))
	vb.add_child(spacer)

	_again_btn = Button.new()
	_again_btn.text = "↻  Play Again  [Enter/R]"
	_again_btn.custom_minimum_size = Responsive.button_min(Vector2(240, 44), self)
	_again_btn.add_theme_font_size_override("font_size", Responsive.font(18, self))
	_again_btn.pressed.connect(_on_again)
	vb.add_child(_again_btn)

	_menu_btn = Button.new()
	_menu_btn.text = "☰  Main Menu  [M/Esc]"
	_menu_btn.custom_minimum_size = Responsive.button_min(Vector2(240, 44), self)
	_menu_btn.add_theme_font_size_override("font_size", Responsive.font(18, self))
	_menu_btn.pressed.connect(_on_menu)
	vb.add_child(_menu_btn)


func _bind() -> void:
	Context.bind_property(_headline, "text", vm, "headline")
	Context.bind_property(_score, "text", vm, "score_label")
	Context.bind_property(_best, "text", vm, "best_label")
	Context.bind_property(_new_record, "text", vm, "new_record_label")


func _on_again() -> void:
	start_activity_with("game", Intent.FLAG_NEW_CLEAR)


func _on_menu() -> void:
	start_activity_with("main_menu", Intent.FLAG_NEW_CLEAR)


func _setup_keyboard() -> void:
	if not EIHelpers.available():
		return
	_ia_again = EIHelpers.make_bool_action("GameOver.PlayAgain")
	_ia_menu = EIHelpers.make_bool_action("GameOver.MainMenu")
	_imc = EIHelpers.make_context("IMC_GameOver")
	_imc.add_mapping(EIHelpers.key_event(KEY_ENTER), _ia_again, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_R), _ia_again, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_M), _ia_menu, [], [], true)
	_imc.add_mapping(EIHelpers.key_event(KEY_ESCAPE), _ia_menu, [], [], true)

	_ei_component = EIHelpers.attach_component(self)
	_ei_component.bind(_ia_again, EIHelpers.TRIG_STARTED, _on_again)
	_ei_component.bind(_ia_menu, EIHelpers.TRIG_STARTED, _on_menu)
