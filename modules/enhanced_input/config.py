def can_build(env, platform):
	# Enhanced Input is pure C++ with no platform-specific dependencies.
	# It is built on all platforms where the engine can build.
	return True


def configure(env):
	# No platform-specific configuration required in P1.
	pass


def get_doc_classes():
	# P5+ will return ["EISubsystem", "EIAction", "EIMappingContext",
	# "EIModifier", "EITrigger"] once those classes are registered.
	return []


def get_doc_path():
	return "doc_classes"
