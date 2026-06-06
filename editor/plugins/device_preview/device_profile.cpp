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

DeviceProfile::DeviceProfile() {
}
