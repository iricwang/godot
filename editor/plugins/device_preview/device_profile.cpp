/**************************************************************************/
/*  device_profile.cpp                                                    */
/**************************************************************************/

#include "device_profile.h"

#include "core/object/class_db.h"

void DeviceProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_device_name", "name"), &DeviceProfile::set_device_name);
	ClassDB::bind_method(D_METHOD("get_device_name"), &DeviceProfile::get_device_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_name"), "set_device_name", "get_device_name");

	ClassDB::bind_method(D_METHOD("set_resolution", "resolution"), &DeviceProfile::set_resolution);
	ClassDB::bind_method(D_METHOD("get_resolution"), &DeviceProfile::get_resolution);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "resolution"), "set_resolution", "get_resolution");

	ClassDB::bind_method(D_METHOD("set_dpi", "dpi"), &DeviceProfile::set_dpi);
	ClassDB::bind_method(D_METHOD("get_dpi"), &DeviceProfile::get_dpi);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dpi"), "set_dpi", "get_dpi");

	ClassDB::bind_method(D_METHOD("set_safe_area", "area"), &DeviceProfile::set_safe_area);
	ClassDB::bind_method(D_METHOD("get_safe_area"), &DeviceProfile::get_safe_area);
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "safe_area"), "set_safe_area", "get_safe_area");

	ClassDB::bind_method(D_METHOD("set_device_class", "device_class"), &DeviceProfile::set_device_class);
	ClassDB::bind_method(D_METHOD("get_device_class"), &DeviceProfile::get_device_class);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "device_class", PROPERTY_HINT_ENUM, "Generic,Phone,Tablet,Desktop,Console"), "set_device_class", "get_device_class");

	ClassDB::bind_method(D_METHOD("set_feature_tags", "tags"), &DeviceProfile::set_feature_tags);
	ClassDB::bind_method(D_METHOD("get_feature_tags"), &DeviceProfile::get_feature_tags);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "feature_tags"), "set_feature_tags", "get_feature_tags");

	ClassDB::bind_method(D_METHOD("get_effective_feature_tags"), &DeviceProfile::get_effective_feature_tags);

	BIND_ENUM_CONSTANT(DEVICE_CLASS_GENERIC);
	BIND_ENUM_CONSTANT(DEVICE_CLASS_PHONE);
	BIND_ENUM_CONSTANT(DEVICE_CLASS_TABLET);
	BIND_ENUM_CONSTANT(DEVICE_CLASS_DESKTOP);
	BIND_ENUM_CONSTANT(DEVICE_CLASS_CONSOLE);
}

void DeviceProfile::set_device_name(const String &p_name) {
	device_name = p_name;
}

String DeviceProfile::get_device_name() const {
	return device_name;
}

void DeviceProfile::set_resolution(const Vector2i &p_resolution) {
	resolution = p_resolution;
}

Vector2i DeviceProfile::get_resolution() const {
	return resolution;
}

void DeviceProfile::set_dpi(float p_dpi) {
	dpi = p_dpi;
}

float DeviceProfile::get_dpi() const {
	return dpi;
}

void DeviceProfile::set_safe_area(const Rect2 &p_area) {
	safe_area = p_area;
}

Rect2 DeviceProfile::get_safe_area() const {
	return safe_area;
}

void DeviceProfile::set_device_class(DeviceClass p_class) {
	device_class = p_class;
}

DeviceProfile::DeviceClass DeviceProfile::get_device_class() const {
	return device_class;
}

void DeviceProfile::set_feature_tags(const PackedStringArray &p_tags) {
	feature_tags = p_tags;
}

PackedStringArray DeviceProfile::get_feature_tags() const {
	return feature_tags;
}

PackedStringArray DeviceProfile::get_effective_feature_tags() const {
	// Explicit override wins. Tags are lowercased to match Godot's
	// feature-tag convention (OS::has_feature is case-sensitive against
	// the lowercase canonical form).
	if (!feature_tags.is_empty()) {
		PackedStringArray normalized;
		normalized.resize(feature_tags.size());
		for (int i = 0; i < feature_tags.size(); ++i) {
			normalized.set(i, feature_tags[i].strip_edges().to_lower());
		}
		return normalized;
	}

	PackedStringArray defaults;
	switch (device_class) {
		case DEVICE_CLASS_PHONE:
		case DEVICE_CLASS_TABLET:
			defaults.push_back("mobile");
			break;
		case DEVICE_CLASS_DESKTOP:
			defaults.push_back("pc");
			break;
		case DEVICE_CLASS_CONSOLE:
		case DEVICE_CLASS_GENERIC:
			break;
	}
	return defaults;
}

DeviceProfile::DeviceProfile() {
}
