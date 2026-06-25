extends "res://core/base_view_model.gd"
## MainMenuVM — title / subtitle / best-time line + total-runs counter.

func _init() -> void:
	self.title = "FortySix"
	self.subtitle = "Click your way to 46"
	self.best_label = "Best: —"
	self.runs_label = "Runs: 0"


func refresh_from_state(game_state) -> void:
	if game_state.has_record():
		self.best_label = "Best: %.2fs (%s)" % [game_state.best_time, game_state.get_difficulty_name()]
	else:
		self.best_label = "Best: — (%s)" % game_state.get_difficulty_name()
	self.runs_label = "Runs: %d" % game_state.total_runs
