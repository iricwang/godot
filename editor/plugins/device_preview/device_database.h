/**************************************************************************/
/*  device_database.h                                                     */
/**************************************************************************/
#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "device_profile.h"

class DeviceDatabase {


public:
	/// Returns the singleton list of built-in device presets.
	static const Vector<Ref<DeviceProfile>> &get_presets();

	/// Look up a preset by display name; returns null if not found.
	static Ref<DeviceProfile> find_by_name(const String &p_name);

private:
	static Vector<Ref<DeviceProfile>> _build_presets();
	static Vector<Ref<DeviceProfile>> presets;
};
