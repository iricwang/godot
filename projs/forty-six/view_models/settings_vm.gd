extends "res://core/base_view_model.gd"
## SettingsVM — difficulty index + volume + their pretty labels.

func _init() -> void:
	self.difficulty = 1
	self.difficulty_label = "Normal (46)"
	self.volume = 80
	self.volume_label = "Volume: 80%"


func sync_from(game_state) -> void:
	begin_bulk_update()
	self.difficulty = game_state.difficulty
	self.difficulty_label = game_state.get_difficulty_name()
	self.volume = game_state.volume
	self.volume_label = "Volume: %d%%" % game_state.volume
	end_bulk_update()


func set_difficulty_local(idx: int, name: String) -> void:
	begin_bulk_update()
	self.difficulty = idx
	self.difficulty_label = name
	end_bulk_update()


func set_volume_local(percent: int) -> void:
	begin_bulk_update()
	self.volume = percent
	self.volume_label = "Volume: %d%%" % percent
	end_bulk_update()
