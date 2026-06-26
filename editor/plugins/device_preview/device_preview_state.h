/**************************************************************************/
/*  device_preview_state.h                                                */
/**************************************************************************/
#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "device_profile.h"

// Shared editor-only state: which device preset (if any) and orientation
// are currently selected. The 2D editor's "Res" toolbar and the Game
// workspace's Resolution menu both read & write through this singleton so
// picking iPhone 15 Pro in one immediately reflects in the other.
//
// Persistence: writes through to
//   EditorSettings::project_metadata("device_preview", "device")    : String
//   EditorSettings::project_metadata("device_preview", "landscape") : bool
// so the selection survives editor restarts.
//
// Migration: on first load we ALSO consult the legacy
// `game_view/preview_resolution_device` and
// `game_view/preview_resolution_landscape` keys (which is where the Game
// workspace stored the selection before this singleton existed) so users
// don't lose their last choice.
class DevicePreviewState : public Object {
	GDCLASS(DevicePreviewState, Object);

	static DevicePreviewState *singleton;

	String current_device_name; // "" means no device (Custom / Project / Free).
	bool landscape = false;
	bool _loaded = false;

	void _ensure_loaded();
	void _persist();

protected:
	static void _bind_methods();

public:
	static DevicePreviewState *get_singleton() { return singleton; }

	// True when a real device is selected (not Custom / Project / Free).
	bool has_device() const;

	// Returns "" when no device is selected.
	String get_device_name() const;
	Ref<DeviceProfile> get_profile() const;
	// Native (portrait) resolution of the selected device, or Size2i() when none.
	Size2i get_resolution() const;
	// Resolution after the landscape toggle is applied.
	Size2i get_resolution_oriented() const;

	bool is_landscape() const;

	// Mutators. Each emits `state_changed` only when the resulting state
	// actually differs from the previous one, so listeners don't re-render
	// on no-op writes.
	void set_device_name(const String &p_name);
	void set_landscape(bool p_landscape);
	void clear_device();

	DevicePreviewState();
	~DevicePreviewState();
};
