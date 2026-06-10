extends Control

# Plinko Game Main Menu
# Mobile portrait resolution: 1080x1920

@onready var title_label: Label = $VBoxContainer/TitleContainer/TitleLabel
@onready var play_button: Button = $VBoxContainer/MenuContainer/PlayButton
@onready var settings_button: Button = $VBoxContainer/MenuContainer/SettingsButton
@onready var high_score_label: Label = $VBoxContainer/ScoreContainer/HighScoreLabel
@onready var coins_label: Label = $VBoxContainer/ScoreContainer/CoinsLabel
@onready var background_board: Control = $BackgroundBoard
@onready var anim_player: AnimationPlayer = $AnimationPlayer

var high_score: int = 0
var coins: int = 1000

func _ready() -> void:
	# Set display resolution for mobile portrait
	display_size_for_mobile()
	
	# Setup UI elements
	setup_buttons()
	setup_labels()
	
	# Generate background peg board preview
	generate_peg_preview()
	
	# Start entrance animation
	play_entrance_animation()
	
	# Connect signals
	play_button.pressed.connect(_on_play_pressed)
	settings_button.pressed.connect(_on_settings_pressed)

func display_size_for_mobile() -> void:
	# Configure for 1080x1920 portrait mobile
	var viewport = get_viewport()
	viewport.content_scale_size = Vector2i(1080, 1920)
	viewport.content_scale_mode = Window.CONTENT_SCALE_MODE_CANVAS
	viewport.content_scale_aspect = Window.CONTENT_SCALE_ASPECT_KEEP

func setup_buttons() -> void:
	# Style play button (primary action)
	var play_style = StyleBoxFlat.new()
	play_style.bg_color = Color(0.95, 0.75, 0.15, 1.0)  # Gold color
	play_style.corner_radius_top_left = 30
	play_style.corner_radius_top_right = 30
	play_style.corner_radius_bottom_left = 30
	play_style.corner_radius_bottom_right = 30
	play_style.content_margin_left = 60
	play_style.content_margin_right = 60
	play_style.content_margin_top = 25
	play_style.content_margin_bottom = 25
	play_button.add_theme_stylebox_override("normal", play_style)
	
	var play_hover = StyleBoxFlat.new()
	play_hover.bg_color = Color(1.0, 0.85, 0.3, 1.0)
	play_hover.corner_radius_top_left = 30
	play_hover.corner_radius_top_right = 30
	play_hover.corner_radius_bottom_left = 30
	play_hover.corner_radius_bottom_right = 30
	play_button.add_theme_stylebox_override("hover", play_hover)
	
	# Style settings button (secondary action)
	var settings_style = StyleBoxFlat.new()
	settings_style.bg_color = Color(0.2, 0.2, 0.25, 0.8)
	settings_style.corner_radius_top_left = 25
	settings_style.corner_radius_top_right = 25
	settings_style.corner_radius_bottom_left = 25
	settings_style.corner_radius_bottom_right = 25
	settings_style.content_margin_left = 40
	settings_style.content_margin_right = 40
	settings_style.content_margin_top = 15
	settings_style.content_margin_bottom = 15
	settings_button.add_theme_stylebox_override("normal", settings_style)
	
	var settings_hover = StyleBoxFlat.new()
	settings_hover.bg_color = Color(0.3, 0.3, 0.35, 0.9)
	settings_hover.corner_radius_top_left = 25
	settings_hover.corner_radius_top_right = 25
	settings_hover.corner_radius_bottom_left = 25
	settings_hover.corner_radius_bottom_right = 25
	settings_button.add_theme_stylebox_override("hover", settings_hover)

func setup_labels() -> void:
	# Title styling
	title_label.add_theme_color_override("font_color", Color(1.0, 0.95, 0.8, 1.0))
	title_label.add_theme_font_size_override("font_size", 72)
	
	# Score labels
	high_score_label.add_theme_color_override("font_color", Color(1.0, 0.9, 0.6, 1.0))
	high_score_label.add_theme_font_size_override("font_size", 28)
	
	coins_label.add_theme_color_override("font_color", Color(1.0, 0.8, 0.2, 1.0))
	coins_label.add_theme_font_size_override("font_size", 32)
	
	# Button text
	play_button.add_theme_color_override("font_color", Color(0.15, 0.1, 0.05, 1.0))
	play_button.add_theme_font_size_override("font_size", 36)
	
	settings_button.add_theme_color_override("font_color", Color(0.9, 0.9, 0.9, 1.0))
	settings_button.add_theme_font_size_override("font_size", 24)
	
	# Update displayed values
	update_score_display()

func update_score_display() -> void:
	high_score_label.text = "最高分: %d" % high_score
	coins_label.text = "💰 %d" % coins

func generate_peg_preview() -> void:
	# Create a visual preview of the Plinko peg board in the background
	var peg_container = Control.new()
	peg_container.name = "PegContainer"
	peg_container.set_anchors_preset(Control.PRESET_FULL_RECT)
	background_board.add_child(peg_container)
	
	# Create decorative peg dots pattern
	var peg_scene = preload("res://plinko_game/pegs/pegs.tscn")
	# For now, create simple circles as pegs
	var rows = 8
	var pegs_per_row = 9
	var start_y = 350
	var spacing_y = 60
	var spacing_x = 100
	var start_x = 140
	
	for row in range(rows):
		var offset = 0 if row % 2 == 0 else spacing_x / 2
		var pegs_in_row = pegs_per_row if row % 2 == 0 else pegs_per_row - 1
		for col in range(pegs_in_row):
			var peg = ColorRect.new()
			peg.size = Vector2(16, 16)
			peg.color = Color(0.6, 0.6, 0.65, 0.4)
			peg.position = Vector2(start_x + col * spacing_x + offset, start_y + row * spacing_y)
			peg.custom_minimum_size = Vector2(16, 16)
			
			# Make it circular
			var style = StyleBoxFlat.new()
			style.bg_color = Color(0.5, 0.5, 0.55, 0.35)
			style.corner_radius_top_left = 8
			style.corner_radius_top_right = 8
			style.corner_radius_bottom_left = 8
			style.corner_radius_bottom_right = 8
			peg.add_theme_stylebox_override("normal", style)
			
			peg_container.add_child(peg)
	
	# Create slot indicators at bottom
	create_slot_indicators()

func create_slot_indicators() -> void:
	var slots_container = Control.new()
	slots_container.name = "SlotsContainer"
	slots_container.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	slots_container.offset_bottom = -80
	background_board.add_child(slots_container)
	
	var slot_values = [100, 200, 500, 1000, 500, 200, 100]
	var slot_width = 1080.0 / slot_values.size()
	
	for i in range(slot_values.size()):
		var slot = Label.new()
		slot.text = str(slot_values[i])
		slot.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		slot.add_theme_color_override("font_color", Color(1.0, 0.85, 0.3, 0.5))
		slot.add_theme_font_size_override("font_size", 20)
		slot.position = Vector2(i * slot_width + slot_width/2 - 20, 0)
		slots_container.add_child(slot)

func play_entrance_animation() -> void:
	# Play fade-in animation for menu
	modulate = Color(1, 1, 1, 0)
	var tween = create_tween()
	tween.tween_property(self, "modulate", Color(1, 1, 1, 1), 0.8).set_ease(Tween.EASE_OUT)
	
	# Animate elements one by one
	var container = $VBoxContainer
	container.modulate = Color(1, 1, 1, 0)
	container.position.y += 50
	
	var tween2 = create_tween()
	tween2.tween_property(container, "modulate", Color(1, 1, 1, 1), 0.6).set_delay(0.3)
	tween2.tween_property(container, "position", Vector2(container.position.x, container.position.y - 50), 0.4).set_ease(Tween.EASE_OUT)

func _on_play_pressed() -> void:
	# Play button press animation
	var tween = create_tween()
	tween.tween_property(play_button, "scale", Vector2(0.95, 0.95), 0.1)
	tween.tween_property(play_button, "scale", Vector2(1.0, 1.0), 0.1)
	
	# Play click sound (placeholder for actual sound)
	# AudioStreamPlayer.play()
	
	# Transition to game scene
	get_tree().change_scene_to_file("res://plinko_game/scenes/game.tscn")

func _on_settings_pressed() -> void:
	# Settings button animation
	var tween = create_tween()
	tween.tween_property(settings_button, "scale", Vector2(0.95, 0.95), 0.1)
	tween.tween_property(settings_button, "scale", Vector2(1.0, 1.0), 0.1)
	
	# Show settings panel or transition
	show_settings_popup()

func show_settings_popup() -> void:
	# Create a simple settings overlay
	var popup = Control.new()
	popup.name = "SettingsPopup"
	popup.set_anchors_preset(Control.PRESET_FULL_RECT)
	popup.modulate = Color(0, 0, 0, 0.7)
	
	var panel = Panel.new()
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.size = Vector2(600, 400)
	panel.custom_minimum_size = Vector2(600, 400)
	
	var panel_style = StyleBoxFlat.new()
	panel_style.bg_color = Color(0.15, 0.15, 0.2, 0.95)
	panel_style.corner_radius_top_left = 20
	panel_style.corner_radius_top_right = 20
	panel_style.corner_radius_bottom_left = 20
	panel_style.corner_radius_bottom_right = 20
	panel.add_theme_stylebox_override("panel", panel_style)
	
	var close_btn = Button.new()
	close_btn.text = "关闭"
	close_btn.position = Vector2(250, 320)
	close_btn.custom_minimum_size = Vector2(100, 50)
	close_btn.pressed.connect(func(): popup.queue_free())
	
	var settings_label = Label.new()
	settings_label.text = "设置"
	settings_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	settings_label.add_theme_font_size_override("font_size", 36)
	settings_label.add_theme_color_override("font_color", Color(1, 1, 1, 1))
	settings_label.position = Vector2(250, 50)
	
	var sound_label = Label.new()
	sound_label.text = "音量: 80%"
	sound_label.add_theme_font_size_override("font_size", 24)
	sound_label.add_theme_color_override("font_color", Color(0.9, 0.9, 0.9, 1))
	sound_label.position = Vector2(200, 130)
	
	panel.add_child(settings_label)
	panel.add_child(sound_label)
	panel.add_child(close_btn)
	popup.add_child(panel)
	add_child(popup)
	
	# Animate popup appearance
	panel.scale = Vector2(0.8, 0.8)
	panel.modulate = Color(1, 1, 1, 0)
	var tween = create_tween()
	tween.tween_property(panel, "scale", Vector2(1, 1), 0.3).set_ease(Tween.EASE_OUT)
	tween.parallel().tween_property(panel, "modulate", Color(1, 1, 1, 1), 0.3)