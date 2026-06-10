extends Control

# Plinko Game Scene - Placeholder
# Resolution: 1080x1920 (Mobile Portrait)

@onready var drop_area: Control = $DropArea
@onready var peg_container: Control = $PegContainer
@onready var slots_container: Control = $SlotsContainer
@onready var score_label: Label = $UI/ScoreLabel
@onready var balls_label: Label = $UI/BallsLabel
@onready var back_button: Button = $UI/BackButton

var score: int = 0
var balls: int = 10
var pegs: Array = []
var slots: Array = []

const ROWS = 10
const PEGS_PER_ROW = 9
const PEG_SIZE = 18
const ROW_SPACING = 70
const COL_SPACING = 90
const BOARD_TOP = 180
const BOARD_LEFT = 90

func _ready() -> void:
	setup_game_board()
	setup_ui()
	connect_signals()

func setup_game_board() -> void:
	# Create pegs in triangular pattern
	for row in range(ROWS):
		var pegs_in_row = PEGS_PER_ROW if row % 2 == 0 else PEGS_PER_ROW - 1
		var offset = ROW_SPACING / 2 if row % 2 == 1 else 0
		
		for col in range(pegs_in_row):
			var peg = ColorRect.new()
			peg.size = Vector2(PEG_SIZE, PEG_SIZE)
			peg.color = Color(0.7, 0.7, 0.75, 1.0)
			peg.position = Vector2(
				BOARD_LEFT + col * COL_SPACING + offset,
				BOARD_TOP + row * ROW_SPACING
			)
			
			# Style the peg
			var style = StyleBoxFlat.new()
			style.bg_color = Color(0.65, 0.65, 0.7, 1.0)
			style.corner_radius_top_left = PEG_SIZE / 2
			style.corner_radius_top_right = PEG_SIZE / 2
			style.corner_radius_bottom_left = PEG_SIZE / 2
			style.corner_radius_bottom_right = PEG_SIZE / 2
			peg.add_theme_stylebox_override("normal", style)
			
			peg_container.add_child(peg)
			pegs.append(peg)
	
	# Create slots at bottom
	create_slots()

func create_slots() -> void:
	var slot_values = [50, 100, 200, 500, 1000, 500, 200, 100, 50]
	var slot_width = 900.0 / slot_values.size()
	var slot_height = 80.0
	var slot_top = BOARD_TOP + ROWS * ROW_SPACING + 20
	
	for i in range(slot_values.size()):
		var slot = Panel.new()
		slot.size = Vector2(slot_width - 5, slot_height)
		slot.position = Vector2(BOARD_LEFT + i * slot_width + 2, slot_top)
		
		# Style slot
		var style = StyleBoxFlat.new()
		var color: Color
		if slot_values[i] >= 500:
			color = Color(1.0, 0.8, 0.1, 0.9)  # Gold for high values
		elif slot_values[i] >= 200:
			color = Color(0.3, 0.6, 0.9, 0.9)  # Blue for medium
		else:
			color = Color(0.4, 0.4, 0.5, 0.9)  # Gray for low
		style.bg_color = color
		style.corner_radius_top_left = 8
		style.corner_radius_top_right = 8
		style.corner_radius_bottom_left = 8
		style.corner_radius_bottom_right = 8
		slot.add_theme_stylebox_override("panel", style)
		
		# Add value label
		var label = Label.new()
		label.text = str(slot_values[i])
		label.horizontal_align = HORIZONTAL_ALIGNMENT_CENTER
		label.vertical_align = VERTICAL_ALIGNMENT_CENTER
		label.add_theme_color_override("font_color", Color(1, 1, 1, 1))
		label.add_theme_font_size_override("font_size", 20)
		label.set_anchors_preset(Control.PRESET_FULL_RECT)
		slot.add_child(label)
		
		slots_container.add_child(slot)
		slots.append({"panel": slot, "value": slot_values[i], "index": i})

func setup_ui() -> void:
	# Score display
	score_label.text = "分数: %d" % score
	score_label.add_theme_color_override("font_color", Color(1, 0.95, 0.7, 1))
	score_label.add_theme_font_size_override("font_size", 32)
	
	# Balls remaining
	balls_label.text = "剩余: %d" % balls
	balls_label.add_theme_color_override("font_color", Color(0.9, 0.9, 0.95, 1))
	balls_label.add_theme_font_size_override("font_size", 28)
	
	# Back button
	var back_style = StyleBoxFlat.new()
	back_style.bg_color = Color(0.2, 0.2, 0.25, 0.9)
	back_style.corner_radius_top_left = 20
	back_style.corner_radius_top_right = 20
	back_style.corner_radius_bottom_left = 20
	back_style.corner_radius_bottom_right = 20
	back_button.add_theme_stylebox_override("normal", back_style)
	back_button.add_theme_color_override("font_color", Color(0.9, 0.9, 0.9, 1))
	back_button.add_theme_font_size_override("font_size", 24)

func connect_signals() -> void:
	back_button.pressed.connect(_on_back_pressed)
	drop_area.gui_input.connect(_on_drop_area_input)

func _on_drop_area_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch and event.pressed and balls > 0:
		drop_ball(event.position)

func drop_ball(pos: Vector2) -> void:
	balls -= 1
	balls_label.text = "剩余: %d" % balls
	
	# Create ball
	var ball = ColorRect.new()
	ball.size = Vector2(30, 30)
	ball.color = Color(0.95, 0.75, 0.15, 1.0)
	
	# Make ball circular
	var ball_style = StyleBoxFlat.new()
	ball_style.bg_color = Color(1.0, 0.85, 0.2, 1.0)
	ball_style.corner_radius_top_left = 15
	ball_style.corner_radius_top_right = 15
	ball_style.corner_radius_bottom_left = 15
	ball_style.corner_radius_bottom_right = 15
	ball.add_theme_stylebox_override("normal", ball_style)
	
	ball.position = pos - Vector2(15, 15)
	drop_area.add_child(ball)
	
	# Animate ball falling
	animate_ball_fall(ball, pos.x)

func animate_ball_fall(ball: ColorRect, start_x: float) -> void:
	var current_x = start_x
	var current_y = BOARD_TOP - 30
	var velocity_x = randf_range(-30, 30)
	
	# Simple physics simulation
	var tween = create_tween().set_loops(false)
	
	# Create path animation
	var duration = 2.5
	var steps = 50
	var step_duration = duration / steps
	
	for i in range(steps):
		var row = int((current_y - BOARD_TOP) / ROW_SPACING)
		
		# Bounce off pegs
		if row >= 0 and row < ROWS:
			var pegs_in_row = PEGS_PER_ROW if row % 2 == 0 else PEGS_PER_ROW - 1
			var offset = ROW_SPACING / 2 if row % 2 == 1 else 0
			
			# Check collision with pegs
			for peg in pegs:
				if peg.position.y >= current_y - PEG_SIZE and peg.position.y <= current_y + PEG_SIZE:
					var peg_x = peg.position.x + PEG_SIZE / 2
					if abs(current_x - peg_x) < PEG_SIZE:
						velocity_x = (current_x - peg_x) * 0.8
						break
		
		# Apply gravity and randomness
		velocity_x += randf_range(-20, 20)
		velocity_x = clamp(velocity_x, -100, 100)
		current_x += velocity_x * step_duration
		current_y += 150 * step_duration
		
		# Clamp x position
		current_x = clamp(current_x, BOARD_LEFT, BOARD_LEFT + (PEGS_PER_ROW - 1) * COL_SPACING)
	
	# Simplified animation - just drop straight down with some wobble
	var final_x = BOARD_LEFT + (PEGS_PER_ROW - 1) * COL_SPACING / 2
	var target_y = BOARD_TOP + ROWS * ROW_SPACING + 60
	
	# Animate with wobble
	var final_tween = create_tween()
	final_tween.tween_property(ball, "position", Vector2(final_x, target_y), 2.0).set_ease(Tween.EASE_IN).set_trans(Tween.TRANS_QUAD)
	
	# Add some horizontal movement
	var wobble_tween = create_tween()
	wobble_tween.tween_property(ball, "position:x", final_x + randf_range(-50, 50), 1.0)
	wobble_tween.tween_property(ball, "position:x", final_x + randf_range(-30, 30), 0.8)
	wobble_tween.tween_property(ball, "position:x", final_x, 0.2)
	
	# On landing, update score
	await get_tree().create_timer(2.0).timeout
	var slot_index = randi() % slots.size()
	var points = slots[slot_index].value
	score += points
	score_label.text = "分数: %d" % score
	
	# Flash the slot
	var slot_panel = slots[slot_index].panel
	var flash_tween = create_tween()
	flash_tween.tween_property(slot_panel, "modulate", Color(1.5, 1.5, 0.5, 1.5), 0.2)
	flash_tween.tween_property(slot_panel, "modulate", Color(1, 1, 1, 1), 0.3)
	
	ball.queue_free()

func _on_back_pressed() -> void:
	get_tree().change_scene_to_file("res://plinko_game/scenes/main_menu.tscn")