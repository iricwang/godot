/**************************************************************************/
/*  device_preview_state.cpp                                              */
/**************************************************************************/

#include "device_preview_state.h"

#include "device_database.h"
#include "editor/settings/editor_settings.h"

DevicePreviewState *DevicePreviewState::singleton = nullptr;

void DevicePreviewState::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_device_name"), &DevicePreviewState::get_device_name);
	ClassDB::bind_method(D_METHOD("set_device_name", "name"), &DevicePreviewState::set_device_name);
	ClassDB::bind_method(D_METHOD("is_landscape"), &DevicePreviewState::is_landscape);
	ClassDB::bind_method(D_METHOD("set_landscape", "landscape"), &DevicePreviewState::set_landscape);
	ClassDB::bind_method(D_METHOD("has_device"), &DevicePreviewState::has_device);
	ClassDB::bind_method(D_METHOD("clear_device"), &DevicePreviewState::clear_device);
	ClassDB::bind_method(D_METHOD("get_profile"), &DevicePreviewState::get_profile);
	ClassDB::bind_method(D_METHOD("get_resolution"), &DevicePreviewState::get_resolution);
	ClassDB::bind_method(D_METHOD("get_resolution_oriented"), &DevicePreviewState::get_resolution_oriented);

	// Emitted whenever device_name or landscape actually changed. Listeners
	// should treat each emission as "re-read everything"; payload is empty
	// to keep observers simple (they all need to refresh anyway).
	ADD_SIGNAL(MethodInfo("state_changed"));
}

void DevicePreviewState::_ensure_loaded() {
	if (_loaded) {
		return;
	}
	_loaded = true;

	EditorSettings *es = EditorSettings::get_singleton();
	if (!es) {
		// No settings yet (very early init); next mutation will re-attempt.
		_loaded = false;
		return;
	}

	// New canonical keys.
	current_device_name = es->get_project_metadata("device_preview", "device", String());
	landscape = es->get_project_metadata("device_preview", "landscape", false);

	// Legacy migration: the Game workspace stored the same idea under
	// `game_view/preview_resolution_device` before this singleton existed.
	// If the new key is empty but the legacy one isn't, adopt the legacy
	// value (we don't delete the old key so an older binary could still
	// roll back without losing its state).
	if (current_device_name.is_empty()) {
		const String legacy = es->get_project_metadata("game_view", "preview_resolution_device", String());
		if (!legacy.is_empty()) {
			current_device_name = legacy;
		}
		// Same for orientation.
		landscape = es->get_project_metadata("game_view", "preview_resolution_landscape", landscape);
	}

	// Defensive: if the persisted device name doesn't exist in the database
	// anymore (e.g. presets were renamed), drop it silently rather than
	// keep retrying every read.
	if (!current_device_name.is_empty() && DeviceDatabase::find_by_name(current_device_name).is_null()) {
		current_device_name = String();
	}
}

void DevicePreviewState::_persist() {
	EditorSettings *es = EditorSettings::get_singleton();
	if (!es) {
		return;
	}
	es->set_project_metadata("device_preview", "device", current_device_name);
	es->set_project_metadata("device_preview", "landscape", landscape);
}

bool DevicePreviewState::has_device() const {
	const_cast<DevicePreviewState *>(this)->_ensure_loaded();
	return !current_device_name.is_empty();
}

String DevicePreviewState::get_device_name() const {
	const_cast<DevicePreviewState *>(this)->_ensure_loaded();
	return current_device_name;
}

Ref<DeviceProfile> DevicePreviewState::get_profile() const {
	const_cast<DevicePreviewState *>(this)->_ensure_loaded();
	if (current_device_name.is_empty()) {
		return Ref<DeviceProfile>();
	}
	return DeviceDatabase::find_by_name(current_device_name);
}

Size2i DevicePreviewState::get_resolution() const {
	Ref<DeviceProfile> p = get_profile();
	if (p.is_null()) {
		return Size2i();
	}
	return p->get_resolution();
}

Size2i DevicePreviewState::get_resolution_oriented() const {
	Size2i res = get_resolution();
	if (res == Size2i() || res.x == res.y) {
		return res;
	}
	const bool native_is_landscape = res.x > res.y;
	if (native_is_landscape != landscape) {
		SWAP(res.x, res.y);
	}
	return res;
}

bool DevicePreviewState::is_landscape() const {
	const_cast<DevicePreviewState *>(this)->_ensure_loaded();
	return landscape;
}

void DevicePreviewState::set_device_name(const String &p_name) {
	_ensure_loaded();
	// Normalize: a non-existent device name reduces to "" so the rest of
	// the state machine stays simple ("either we have a real profile or
	// we don't").
	String name = p_name;
	if (!name.is_empty() && DeviceDatabase::find_by_name(name).is_null()) {
		name = String();
	}
	if (name == current_device_name) {
		return;
	}
	current_device_name = name;
	_persist();
	emit_signal(SNAME("state_changed"));
}

void DevicePreviewState::set_landscape(bool p_landscape) {
	_ensure_loaded();
	if (p_landscape == landscape) {
		return;
	}
	landscape = p_landscape;
	_persist();
	emit_signal(SNAME("state_changed"));
}

void DevicePreviewState::clear_device() {
	set_device_name(String());
}

DevicePreviewState::DevicePreviewState() {
	singleton = this;
}

DevicePreviewState::~DevicePreviewState() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
