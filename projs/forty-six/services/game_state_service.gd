extends RefCounted
## GameStateService — persistent game state held in the [ServiceRegistry].
##
## Registered under the key &"game_state" in [entry.gd]. Anywhere with a
## Context handle (Activity / Dialog / Toast) can fetch it via
## [code]get_service(&"game_state")[/code].
##
## Properties:
##   * best_time      — fastest completion in seconds (lower is better; INF = no record).
##   * total_runs     — number of completed runs since boot.
##   * difficulty     — 0 Easy(23) / 1 Normal(46) / 2 Hard(92).
##   * volume         — 0..100, also published to AudioService when changed.
##
## This service intentionally has NO save-to-disk; it's the in-process source
## of truth. A real game would persist to user:// in `_notification(WM_CLOSE_REQUEST)`.

const DIFFICULTY_NAMES := ["Easy (23)", "Normal (46)", "Hard (92)"]
const DIFFICULTY_TARGETS := [23, 46, 92]

var best_time: float = INF
var total_runs: int = 0
var difficulty: int = 1  # Normal
var volume: int = 80


func get_target_clicks() -> int:
	return DIFFICULTY_TARGETS[difficulty]


func get_difficulty_name() -> String:
	return DIFFICULTY_NAMES[difficulty]


## Records a completed run. Returns true iff `seconds` set a new best.
func record_run(seconds: float) -> bool:
	total_runs += 1
	if seconds < best_time:
		best_time = seconds
		return true
	return false


func reset_best() -> void:
	best_time = INF


func has_record() -> bool:
	return best_time != INF
