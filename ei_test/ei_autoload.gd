extends Node
# Autoload script that wires C++ EISubsystem input hook to GDScript.
# C++ Node subclasses cannot override _input directly (Node._input is
# dispatched dynamically to GDScript subclasses), so this GDScript shim
# forwards _input to EISubsystem.
#
# The C++ EISubsystem is registered with ClassDB by the engine module,
# but no instance is created at module init (that would leak in test
# mode). This autoload is the natural owner: it instantiates the C++
# subsystem, parents it to itself so the instance lives inside the
# SceneTree, and publishes it as an Engine singleton so other code
# can find it via `Engine.get_singleton("EISubsystem")`.

var _ei_subsystem: Node = null

func _ready() -> void:
	# If something else already registered the singleton (e.g. a future
	# C++-side init), use that. Otherwise create the C++ instance here.
	var ei = Engine.get_singleton("EISubsystem")
	if ei == null and ClassDB.class_exists("EISubsystem"):
		_ei_subsystem = ClassDB.instantiate("EISubsystem")
		if _ei_subsystem:
			add_child(_ei_subsystem)
			Engine.register_singleton("EISubsystem", _ei_subsystem)
		ei = _ei_subsystem

	print("[ei_test] EISubsystem autoload ready: ", ei != null)
	if ei:
		print("[ei_test] ClassDB has EISubsystem: ", ClassDB.class_exists("EISubsystem"))
		print("[ei_test] log_level before: ", ei.log_level)
		ei.log_level = 2

func _input(event: InputEvent) -> void:
	# In P5 this would call ei.inject_input(event). For P1 we just log.
	var ei = Engine.get_singleton("EISubsystem")
	if ei and ei.log_level >= 2:
		print("[ei_test] forwarded event: ", event)
