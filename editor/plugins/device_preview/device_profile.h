/**************************************************************************/
/*  device_profile.h                                                      */
/**************************************************************************/
#pragma once

#include "core/io/resource.h"
#include "core/math/rect2.h"
#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "core/variant/typed_array.h"

class DeviceProfile : public Resource {
	GDCLASS(DeviceProfile, Resource);

public:
	// Broad device classification. Drives the default feature-tag set when
	// `feature_tags` is left empty, and lets the preview UI group entries.
	// Keep numeric values stable: they are persisted via project metadata.
	enum DeviceClass {
		DEVICE_CLASS_GENERIC = 0,
		DEVICE_CLASS_PHONE = 1,
		DEVICE_CLASS_TABLET = 2,
		DEVICE_CLASS_DESKTOP = 3,
		DEVICE_CLASS_CONSOLE = 4,
	};

private:
	String device_name;
	Vector2i resolution = Vector2i(1080, 1920);
	float dpi = 320.0f;
	Rect2 safe_area; // In pixel coordinates, relative to top-left (0,0).
	DeviceClass device_class = DEVICE_CLASS_GENERIC;
	// When non-empty, these tags take precedence over the class-derived
	// defaults. Always lowercase to match Godot's feature-tag convention.
	PackedStringArray feature_tags;

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

	void set_device_class(DeviceClass p_class);
	DeviceClass get_device_class() const;

	void set_feature_tags(const PackedStringArray &p_tags);
	PackedStringArray get_feature_tags() const;

	// Returns `feature_tags` when non-empty, otherwise the default tag set
	// derived from `device_class`:
	//   PHONE/TABLET  -> ["mobile"]
	//   DESKTOP       -> ["pc"]
	//   CONSOLE       -> []   (left to the project to define)
	//   GENERIC       -> []
	// PC-host platforms already report "pc" via OS::has_feature, so the
	// canonical "is this the mobile branch?" check stays `OS.has_feature("mobile")`.
	PackedStringArray get_effective_feature_tags() const;

	DeviceProfile();
};

VARIANT_ENUM_CAST(DeviceProfile::DeviceClass);
