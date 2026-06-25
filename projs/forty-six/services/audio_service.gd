extends RefCounted
## AudioService — minimal stub registered under &"audio" in the
## [ServiceRegistry]. Real games would route through AudioStreamPlayer here;
## for the demo we just print to confirm the service-discovery wiring.
##
## Usage from any Activity / Dialog:
##   var audio = get_service(&"audio")
##   audio.play_sfx("click")

var muted: bool = false


func play_sfx(name: StringName) -> void:
	if muted:
		return
	print("[AudioService] sfx: ", name)


func set_volume(percent: int) -> void:
	muted = (percent <= 0)
	print("[AudioService] volume = %d%% (muted=%s)" % [percent, muted])
