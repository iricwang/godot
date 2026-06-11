extends Node
# P5/P10 autoload for the plinko_game EI demo.
#
# Responsibilities:
# 1. Lazily create the C++ EISubsystem singleton at boot and publish it
#    as Engine.get_singleton("EISubsystem"). Mirrors ei_test/ei_autoload.gd
#    but written against the final P5 API (inject_input) instead of the
#    P1 log-only stub.
# 2. Forward every InputEvent the engine routes to this Node to the
#    subsystem via inject_input(). The subsystem's dispatcher then
#    walks the active IMCs in priority order and fires trigger events.
#
# Per spec §1.1.1 (Hook model): we sit on the autoload's _input. Higher-
# priority IMCs that mark consumes=true call Viewport.set_input_as_handled()
# so the event does not leak to user scene scripts' _input callbacks
# (this is a property of the dispatcher, not of this autoload — included
# here for context).

var _ei_subsystem: Node = null

func _ready() -> void:
	# Reuse an existing singleton if one was registered externally.
	# `Engine.get_singleton` prints an error to stderr if missing, so we
	# gate on `has_singleton` first to keep the boot log clean.
	var ei: Object = null
	if Engine.has_singleton("EISubsystem"):
		ei = Engine.get_singleton("EISubsystem")
	if ei == null and ClassDB.class_exists("EISubsystem"):
		_ei_subsystem = ClassDB.instantiate("EISubsystem")
		if _ei_subsystem:
			add_child(_ei_subsystem)
			Engine.register_singleton("EISubsystem", _ei_subsystem)
			ei = _ei_subsystem

	if ei:
		# Verbose logging helps the demo's user see the IMC dispatch.
		# 0=silent, 1=event, 2=verbose (per ei_subsystem.h).
		ei.log_level = 2
		print("[plinko_demo] EISubsystem ready (log_level=%d)" % ei.log_level)
	else:
		push_error("[plinko_demo] EISubsystem class missing — enhanced_input module not built?")

func _input(event: InputEvent) -> void:
	var ei := Engine.get_singleton("EISubsystem")
	if ei == null:
		return
	# The dispatcher decides whether to mark the event as handled based
	# on the highest-priority IMC mapping that matched; we just forward
	# everything that bubbles up to the autoload.
	ei.inject_input(event)
