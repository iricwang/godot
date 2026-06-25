/**************************************************************************/
/*  device_database.cpp                                                   */
/**************************************************************************/

#include "device_database.h"

Vector<Ref<DeviceProfile>> DeviceDatabase::presets;

static Ref<DeviceProfile> _make(const String &p_name, int p_w, int p_h, float p_dpi, DeviceProfile::DeviceClass p_class, const Rect2 &p_safe = Rect2()) {
	Ref<DeviceProfile> dp;
	dp.instantiate();
	dp->set_device_name(p_name);
	dp->set_resolution(Vector2i(p_w, p_h));
	dp->set_dpi(p_dpi);
	dp->set_safe_area(p_safe);
	dp->set_device_class(p_class);
	return dp;
}

Vector<Ref<DeviceProfile>> DeviceDatabase::_build_presets() {
	Vector<Ref<DeviceProfile>> list;

	// ---- iPhones ----
	list.push_back(_make("iPhone 15 Pro Max", 430, 932, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 59, 430, 839)));
	list.push_back(_make("iPhone 15 Pro", 393, 852, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 59, 393, 759)));
	list.push_back(_make("iPhone 15", 393, 852, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 54, 393, 764)));
	list.push_back(_make("iPhone 14 Pro Max", 430, 932, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 59, 430, 839)));
	list.push_back(_make("iPhone 14 Pro", 393, 852, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 59, 393, 759)));
	list.push_back(_make("iPhone 14", 390, 844, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 44, 390, 767)));
	list.push_back(_make("iPhone SE (3rd gen)", 375, 667, 326, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("iPhone 13 / 13 Pro", 390, 844, 460, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 44, 390, 767)));
	list.push_back(_make("iPhone 13 mini", 375, 812, 476, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 44, 375, 734)));
	list.push_back(_make("iPhone 11 Pro / XS / X", 375, 812, 458, DeviceProfile::DEVICE_CLASS_PHONE, Rect2(0, 44, 375, 734)));

	// ---- iPads ----
	list.push_back(_make("iPad Pro 12.9\" (6th gen)", 1024, 1366, 264, DeviceProfile::DEVICE_CLASS_TABLET));
	list.push_back(_make("iPad Pro 11\" (4th gen)", 834, 1194, 264, DeviceProfile::DEVICE_CLASS_TABLET));
	list.push_back(_make("iPad Air (5th gen)", 820, 1180, 264, DeviceProfile::DEVICE_CLASS_TABLET));
	list.push_back(_make("iPad (10th gen)", 810, 1080, 264, DeviceProfile::DEVICE_CLASS_TABLET));
	list.push_back(_make("iPad mini (6th gen)", 744, 1133, 326, DeviceProfile::DEVICE_CLASS_TABLET));

	// ---- Android Phones ----
	list.push_back(_make("Samsung Galaxy S24 Ultra", 412, 915, 505, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("Samsung Galaxy S24", 360, 780, 416, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("Google Pixel 8 Pro", 448, 1008, 489, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("Google Pixel 8", 412, 915, 428, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("Google Pixel 7a", 412, 915, 429, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("OnePlus 12", 412, 915, 510, DeviceProfile::DEVICE_CLASS_PHONE));
	list.push_back(_make("Xiaomi 14", 393, 852, 460, DeviceProfile::DEVICE_CLASS_PHONE));

	// ---- Android Tablets ----
	list.push_back(_make("Samsung Galaxy Tab S9", 800, 1280, 274, DeviceProfile::DEVICE_CLASS_TABLET));

	// ---- Desktop ----
	// Marked DESKTOP so selecting one explicitly drives the "pc" branch and
	// shows a [pc] hint in the toolbar. The default `pc` feature tag is also
	// reported by the host platform on Windows/Linux/macOS, so this mostly
	// exists for symmetry with the mobile presets.
	list.push_back(_make("Desktop 1280\xc3\x97""720 (HD)", 1280, 720, 96, DeviceProfile::DEVICE_CLASS_DESKTOP));
	list.push_back(_make("Desktop 1920\xc3\x97""1080 (FHD)", 1920, 1080, 96, DeviceProfile::DEVICE_CLASS_DESKTOP));
	list.push_back(_make("Desktop 2560\xc3\x97""1440 (QHD)", 2560, 1440, 109, DeviceProfile::DEVICE_CLASS_DESKTOP));
	list.push_back(_make("Desktop 3840\xc3\x97""2160 (4K UHD)", 3840, 2160, 163, DeviceProfile::DEVICE_CLASS_DESKTOP));

	// ---- Generic / Common ----
	// Kept untyped (GENERIC -> no injected tag). Selecting one of these
	// leaves the platform-default feature set untouched.
	list.push_back(_make("1080p (FHD)", 1080, 1920, 400, DeviceProfile::DEVICE_CLASS_GENERIC));
	list.push_back(_make("720p (HD)", 720, 1280, 300, DeviceProfile::DEVICE_CLASS_GENERIC));
	list.push_back(_make("1440p (QHD)", 1440, 2560, 500, DeviceProfile::DEVICE_CLASS_GENERIC));
	list.push_back(_make("4K UHD", 2160, 3840, 500, DeviceProfile::DEVICE_CLASS_GENERIC));

	return list;
}

const Vector<Ref<DeviceProfile>> &DeviceDatabase::get_presets() {
	if (presets.is_empty()) {
		presets = _build_presets();
	}
	return presets;
}

Ref<DeviceProfile> DeviceDatabase::find_by_name(const String &p_name) {
	for (const Ref<DeviceProfile> &dp : get_presets()) {
		if (dp->get_device_name() == p_name) {
			return dp;
		}
	}
	return Ref<DeviceProfile>();
}
