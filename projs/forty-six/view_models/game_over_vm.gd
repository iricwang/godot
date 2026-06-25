extends "res://core/base_view_model.gd"
## GameOverVM — score line + best-time line + new-record banner.

func _init() -> void:
	self.headline = "Run complete"
	self.score_label = "0.00s"
	self.best_label = "Best: —"
	self.new_record = false
	self.new_record_label = ""


func apply(score_seconds: float, best_seconds: float, is_new: bool) -> void:
	self.score_label = "Your time: %.2fs" % score_seconds
	if best_seconds == INF:
		self.best_label = "Best: —"
	else:
		self.best_label = "Best: %.2fs" % best_seconds
	self.new_record = is_new
	if is_new:
		self.headline = "New record!"
		self.new_record_label = "⭐  New record"
	else:
		self.headline = "Run complete"
		self.new_record_label = ""
