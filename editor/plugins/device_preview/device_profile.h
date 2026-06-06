/**************************************************************************/
/*  device_profile.h                                                      */
/**************************************************************************/
#pragma once

#include "core/io/resource.h"
#include "core/math/rect2.h"
#include "core/math/vector2i.h"
#include "core/string/ustring.h"

class DeviceProfile : public Resource {
	GDCLASS(DeviceProfile, Resource);

	String device_name;
	Vector2i resolution = Vector2i(1080, 1920);
	float dpi = 320.0f;
	Rect2 safe_area; // In pixel coordinates, relative to top-left (0,0).

protected:
	static void _bind_methods();

public:
	void set_device_name(const String &p_name);
	String get_device_name() const;

	void set_resolution(const Vector2i &p_resolution);
	Vector2i get_resolution() const;

	void set_dpi(float p_dpi);
	float get_dpi() const;

	void set_safe_area(const Rect2 &p_area);
	Rect2 get_safe_area() const;

	DeviceProfile();
};
