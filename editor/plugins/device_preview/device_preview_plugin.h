/**************************************************************************/
/*  device_preview_plugin.h                                               */
/**************************************************************************/
#pragma once

#include "core/math/vector2i.h"
#include "editor/plugins/editor_plugin.h"

class DeviceProfile;
class MenuButton;

class DevicePreviewPlugin : public EditorPlugin {
	GDCLASS(DevicePreviewPlugin, EditorPlugin);

	enum {
		MENU_FREE = 0,
		MENU_CUSTOM = 1,
		MENU_SEPARATOR = 2,
		MENU_DEVICE = 3,
	};

	MenuButton *device_menu = nullptr;

	String current_device_name;
	Ref<DeviceProfile> current_profile;

	Vector2i saved_viewport_size;
	bool preview_active = false;

	// Orientation hint for the preview. When false (default) the device's native
	// resolution is used. When true the resolution is rotated 90 degrees.
	bool preview_resolution_landscape = false;

	void _build_menu();
	void _on_device_selected(int p_id);
	void _apply_preview(const Ref<DeviceProfile> &p_profile);
	void _remove_preview();
	void _update_toolbar_label();
	Vector2i _apply_preview_orientation(const Vector2i &p_size) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual String get_plugin_name() const override { return "DevicePreview"; }
	virtual bool has_main_screen() const override { return false; }

	// Set the orientation for the device preview. When true the device's
	// resolution is rotated 90 degrees (landscape); when false the native
	// resolution is used (portrait for a portrait device). Has no effect
	// when no device is selected.
	void set_preview_resolution_landscape(bool p_landscape);
	bool is_preview_resolution_landscape() const { return preview_resolution_landscape; }

	// Currently applied device resolution (already oriented). Returns
	// Size2i() when no device preview is active.
	Vector2i get_preview_resolution() const;

	DevicePreviewPlugin();
};
