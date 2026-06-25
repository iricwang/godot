extends "res://core/base_view_model.gd"
## GameVM — count / target / elapsed time / progress (0..1) / status.

func _init() -> void:
	self.count = 0
	self.target = 46
	self.target_label = "/ 46"
	self.count_label = "0"
	self.elapsed_label = "0.00s"
	self.progress = 0.0
	self.status = "Click to start the clock"


func set_target(t: int) -> void:
	self.target = t
	self.target_label = "/ %d" % t
	self.progress = float(self.count) / float(t) if t > 0 else 0.0


func bump() -> void:
	# Use begin/end_bulk_update so dependent labels emit a single batched signal.
	begin_bulk_update()
	self.count = int(self.count) + 1
	self.count_label = str(self.count)
	self.progress = float(self.count) / float(self.target) if int(self.target) > 0 else 0.0
	end_bulk_update()


func set_elapsed(seconds: float) -> void:
	self.elapsed_label = "%.2fs" % seconds
