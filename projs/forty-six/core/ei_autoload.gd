extends Node
## EISubsystem autoload — boots the enhanced_input dispatcher and forwards
## window InputEvents into it.
##
## Without this autoload the C++ EISubsystem singleton is never instantiated
## and ClassDB-registered EI types still exist but no event ever reaches the
## mapping contexts. All FortySix Activities check `Engine.has_singleton(...)`
## before they try to add a mapping context; if EI is missing the demo falls
## back to mouse-only.

const SINGLETON_NAME := "EISubsystem"

var _subsystem: Node = null


func _ready() -> void:
	if Engine.has_singleton(SINGLETON_NAME):
		_subsystem = Engine.get_singleton(SINGLETON_NAME)
		return

	if not ClassDB.class_exists(SINGLETON_NAME):
		push_warning("[EIAutoload] enhanced_input module not compiled — keyboard shortcuts disabled.")
		return

	_subsystem = ClassDB.instantiate(SINGLETON_NAME)
	if _subsystem == null:
		push_error("[EIAutoload] ClassDB.instantiate(EISubsystem) returned null")
		return

	add_child(_subsystem)
	Engine.register_singleton(SINGLETON_NAME, _subsystem)
	# 0 silent / 1 event / 2 verbose
	if "log_level" in _subsystem:
		_subsystem.log_level = 2
	print("[EIAutoload] EISubsystem ready")


func _input(event: InputEvent) -> void:
	if _subsystem == null:
		return
	_subsystem.inject_input(event)
