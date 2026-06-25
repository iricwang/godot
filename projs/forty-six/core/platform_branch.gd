## platform_branch.gd — Tiny helper that resolves the active platform branch.
##
## Returns one of:
##   "mobile"  — the editor's device preview is set to a phone/tablet,
##               or this build was launched with `--features mobile`,
##               or the host platform is Android/iOS.
##   "pc"      — desktop host with no active mobile preview.
##   "web"     — running in a browser (HTML5 export).
##
## Resolution order: explicit `mobile` tag wins (because PC hosts hardcode
## `pc` true at the OS layer, so the absence of `mobile` is what actually
## tells you "we are not simulating a phone right now"). See the engine's
## `core/os/os.cpp::OS::has_feature` fallthrough into
## `ProjectSettings::has_custom_feature` for how the Device Preview
## injection bridges into the runtime check.
extends Object
class_name PlatformBranch


## Returns the active branch label as a String.
static func get_active_branch() -> String:
	if OS.has_feature("mobile"):
		return "mobile"
	if OS.has_feature("web"):
		return "web"
	# Default to PC for the desktop host (Windows/Linux/macOS always report `pc`).
	return "pc"


## Convenience flags. Use these in `if` chains instead of stringly-typed checks.
static func is_mobile() -> bool:
	return OS.has_feature("mobile")


static func is_pc() -> bool:
	# Strict "we are NOT simulating a phone right now". This is the canonical
	# desktop check on a PC host where `OS.has_feature("pc")` is always true.
	return not OS.has_feature("mobile") and not OS.has_feature("web")


static func is_web() -> bool:
	return OS.has_feature("web")
