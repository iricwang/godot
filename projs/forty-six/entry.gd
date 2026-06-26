extends Control
## entry.gd — FortySix demo bootstrap.
##
## Builds the [Application], registers two custom services, registers each
## Activity / Dialog scene with the [ActivityManager], paints a dark backdrop,
## then kicks off the MainMenu Activity.
##
## The UIRoot Control IS the activity host: ActivityManager places its
## Activities and Dialogs as children of this node. The [Application] sits
## alongside as a sibling Node so its NOTIFICATION_EXIT_TREE auto-fires
## shutdown() on quit.

const GameStateService := preload("res://services/game_state_service.gd")
const AudioService := preload("res://services/audio_service.gd")
const PlatformBranchScript := preload("res://core/platform_branch.gd")

var app: Application
var _game_state: GameStateService
var _audio: AudioService


func _ready() -> void:
	# Resolve and announce the active platform branch FIRST so the boot log
	# makes the editor-side Device Preview selection observable. Selecting a
	# phone preset in the 2D toolbar or in the Game workspace injects the
	# "mobile" tag into ProjectSettings::custom_features, which OS.has_feature
	# reads via fallthrough; picking a Desktop preset or "Free" reverts the
	# branch to "pc".
	var branch: String = PlatformBranchScript.get_active_branch()
	print("[platform_branch] active branch = %s  (mobile=%s pc=%s web=%s)" % [
		branch,
		OS.has_feature("mobile"),
		OS.has_feature("pc"),
		OS.has_feature("web"),
	])
	print("[viewport] viewport_width=%d viewport_height=%d  content_scale_size=%s  orientation=%d" % [
		ProjectSettings.get_setting("display/window/size/viewport_width"),
		ProjectSettings.get_setting("display/window/size/viewport_height"),
		get_window().content_scale_size,
		ProjectSettings.get_setting("display/window/handheld/orientation"),
	])

	# Dark backdrop so the framed UI reads against the window.
	var bg := ColorRect.new()
	bg.color = Color(0.07, 0.08, 0.1)
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(bg)

	# 1. Application — added as a child so NOTIFICATION_EXIT_TREE fires on quit.
	app = Application.new()
	app.name = "Application"
	add_child(app)
	app.initialize(self)

	# 2. Services — register BEFORE the first Activity reads them.
	_game_state = GameStateService.new()
	_audio = AudioService.new()
	app.get_service_registry().register_service(&"game_state", _game_state)
	app.get_service_registry().register_service(&"audio", _audio)

	# 3. Activity / Dialog registry — the action string is what start_activity
	#    and show_dialog will look up. ActivityManager keys both kinds of
	#    overlays off this single registry.
	app.register_activity("main_menu", "res://activities/main_menu_activity.tscn")
	app.register_activity("game", "res://activities/game_activity.tscn")
	app.register_activity("game_over", "res://activities/game_over_activity.tscn")
	app.register_activity("settings", "res://activities/settings_activity.tscn")

	app.register_activity("pause_dialog", "res://dialogs/pause_dialog.tscn")
	app.register_activity("confirm_quit_dialog", "res://dialogs/confirm_quit_dialog.tscn")

	# 4. Boot — FLAG_LOAD_SYNC so the menu is up on the same frame.
	app.start_activity(Intent.create("main_menu", Intent.FLAG_LOAD_SYNC))

	print("[entry] FortySix boot complete. UIRoot=%s, services=%s" % [
		get_path(),
		app.get_service_registry().get_service_names(),
	])
